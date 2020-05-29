/*
 *
 * Copyright (c) 2020 The Raptor Authors. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include "core/windows/tcp_server.h"
#include "core/windows/tcp_listener.h"
#include "util/alloc.h"
#include "util/cpu.h"
#include "util/log.h"
#include "core/windows/socket_setting.h"

namespace raptor {
enum MessageType {
    kNewConnection,
    kRecvAMessage,
    kCloseClient,
};
struct TcpMessageNode {
    MultiProducerSingleConsumerQueue::Node node;
    MessageType type;
    ConnectionId cid;
    raptor_resolved_address addr;
    Slice slice;
};

constexpr uint32_t InvalidIndex = static_cast<uint32_t>(-1);
TcpServer::TcpServer(ITcpServerService *service, Protocol* proto)
    : _service(service)
    , _proto(proto)
    , _shutdown(true) {
}

TcpServer::~TcpServer() {
    if (!_shutdown) {
        Shutdown();
    }
}

raptor_error TcpServer::Init(const RaptorOptions* options){
    if (!_shutdown) {
        return RAPTOR_ERROR_FROM_STATIC_STRING("tcp server already running");
    }
    _listener = std::make_shared<TcpListener>(this);
    _rs_thread = std::make_shared<SendRecvThread>(this);

    auto e = _listener->Init();
    if (e != RAPTOR_ERROR_NONE) {
        return e;
    }

    e = _rs_thread->Init(2, 0);
    if (e != RAPTOR_ERROR_NONE) {
        return e;
    }

    _shutdown = false;
    _options = *options;
    _count.Store(0);

    _mq_thd = Thread(
            "message_queue",
            std::bind(&TcpServer::MessageQueueThread, this, std::placeholders::_1)
            , nullptr);

    _conn_mtx.Lock();
    _mgr.resize(RESERVED_CONNECTION_COUNT);
    for (size_t i = 0; i < RESERVED_CONNECTION_COUNT; i++) {
        _free_index_list.push_back(i);
    }
    _conn_mtx.Unlock();

    time_t n = Now();
    _magic_number = (n >> 16) & 0xffff;
    _last_timeout_time.Store(n);
    return RAPTOR_ERROR_NONE;
}

raptor_error TcpServer::AddListening(const std::string& addr){
    if (_shutdown) return RAPTOR_ERROR_FROM_STATIC_STRING("tcp server uninitialized");
    if (addr.empty()) return RAPTOR_ERROR_FROM_STATIC_STRING("invalid listening address");
    raptor_resolved_addresses* addrs;
    auto ret = raptor_blocking_resolve_address(addr.c_str(), nullptr, &addrs);
    if (ret != RAPTOR_ERROR_NONE) {
        return ret;
    }

    for (size_t i = 0; i < addrs->naddrs; i++) {
        auto err = _listener->AddListeningPort(&addrs->addrs[i]);
        if (err != RAPTOR_ERROR_NONE) {
            if (ret != RAPTOR_ERROR_NONE) {
                ret->AppendMessage(err->ToString());
            } else {
                ret = err;
            }
        }
    }
    raptor_resolved_addresses_destroy(addrs);
    return ret;
}

raptor_error TcpServer::Start() {
    if (!_listener->Start()) {
        return RAPTOR_ERROR_FROM_STATIC_STRING("failed to start listener");
    }
    if (!_rs_thread->Start()) {
        return RAPTOR_ERROR_FROM_STATIC_STRING("failed to start rs_thread");
    }
    _mq_thd.Start();
    return RAPTOR_ERROR_NONE;
}

void TcpServer::Shutdown(){
    if (!_shutdown) {
        _shutdown = true;
        _listener->Shutdown();
        _rs_thread->Shutdown();
        _cv.Signal();
        _mq_thd.Join();

        _conn_mtx.Lock();
        _timeout_record_list.clear();
        _free_index_list.clear();
        for (auto& obj : _mgr) {
            if (obj.first) {
                obj.first->Shutdown(false);
                delete obj.first;
            }
        }
        _mgr.clear();
        _conn_mtx.Unlock();

        // clear message queue
        bool empty = true;
        do {
            auto n = _mpscq.PopAndCheckEnd(&empty);
            auto msg = reinterpret_cast<TcpMessageNode*>(n);
            if (msg != nullptr) {
                _count.FetchSub(1, MemoryOrder::RELAXED);
                delete msg;
            }
        } while (!empty);
    }
}

bool TcpServer::Send(ConnectionId cid, const void* buf, size_t len) {
    uint32_t index = CheckConnectionId(cid);
    if (index == InvalidIndex) {
        return false;
    }

    if (_mgr[index].first) {
        return _mgr[index].first->Send(buf, len);
    }
    return false;
}

bool TcpServer::CloseConnection(ConnectionId cid){
    uint32_t index = CheckConnectionId(cid);
    if (index == InvalidIndex) {
        return false;
    }

    AutoMutex g(&_conn_mtx);
    if (_mgr[index].first) {
        _mgr[index].first->Shutdown(false);
        DeleteConnection(index);
    }
    return true;
}

// internal::IAcceptor impl
void TcpServer::OnNewConnection(
    SOCKET sock, int listen_port, const raptor_resolved_address* addr) {
    AutoMutex g(&_conn_mtx);

    if (_free_index_list.empty() && _mgr.size() >= _options.max_connections) {
        log_error("The maximum number of connections has been reached: %u", _options.max_connections);
        raptor_set_socket_shutdown(sock);
        return;
    }

    if (_free_index_list.empty()) {
        size_t count = _mgr.size();
        size_t expand = ((count * 2) < _options.max_connections) ? (count * 2) : _options.max_connections;
        _mgr.resize(expand);
        for (size_t i = count; i < expand; i++) {
            _mgr[i].first = nullptr;
            _mgr[i].second = _timeout_record_list.end();
            _free_index_list.push_back(i);
        }
    }

    uint32_t index = _free_index_list.front();
    _free_index_list.pop_front();

    ConnectionId cid = core::BuildConnectionId(
        _magic_number, static_cast<uint16_t>(listen_port), index);

    time_t deadline_second = Now() + _options.connection_timeout;

    std::unique_ptr<Connection> conn (new Connection(this));
    conn->Init(cid, sock, addr);
    conn->SetProtocol(_proto);

    // associate with iocp
    _rs_thread->Add(sock, (void*)cid);

    // submit the first asynchronous read
    if (conn->AsyncRecv()) {
        conn->Shutdown(true);
        _free_index_list.push_back(index);
    } else {
        _mgr[index].first = conn.release();
        _mgr[index].second = _timeout_record_list.insert({deadline_second, index});
    }
}

// internal::IIocpReceiver impl
void TcpServer::OnErrorEvent(void* ptr, size_t ) {
    ConnectionId cid = (ConnectionId)ptr;
    uint32_t index = CheckConnectionId(cid);
    if (index == InvalidIndex) {
        log_error("tcpserver: OnErrorEvent found invalid index, cid = %x", cid);
        return;
    }

    AutoMutex g(&_conn_mtx);
    if (_mgr[index].first) {
        _mgr[index].first->Shutdown(true);
        DeleteConnection(index);
    }
}

void TcpServer::OnRecvEvent(void* ptr, size_t transferred_bytes) {
    ConnectionId cid = (ConnectionId)ptr;
    uint32_t index = CheckConnectionId(cid);
    if (index == InvalidIndex) {
        log_error("tcpserver: OnRecvEvent found invalid index, cid = %x", cid);
        return;
    }

    AutoMutex g(&_conn_mtx);
    if (!_mgr[index].first->OnRecvEvent(transferred_bytes)) {
        log_error("tcpserver: Failed to post async recv");
        _mgr[index].first->Shutdown(true);
        DeleteConnection(index);
    } else {
        time_t deadline_seconds = Now() + _options.connection_timeout;
        _timeout_record_list.erase(_mgr[index].second);
        _mgr[index].second = _timeout_record_list.insert({deadline_seconds, index});
    }
}

void TcpServer::OnSendEvent(void* ptr, size_t transferred_bytes) {
    ConnectionId cid = (ConnectionId)ptr;
    uint32_t index = CheckConnectionId(cid);
    if (index == InvalidIndex) {
        log_error("tcpserver: OnRecvEvent found invalid index, cid = %x", cid);
        return;
    }

    AutoMutex g(&_conn_mtx);
    if (!_mgr[index].first->OnSendEvent(transferred_bytes)) {
        log_error("tcpserver: Failed to post async send");
        _mgr[index].first->Shutdown(true);
        DeleteConnection(index);
    } else {
        time_t deadline_seconds = Now() + _options.connection_timeout;
        _timeout_record_list.erase(_mgr[index].second);
        _mgr[index].second = _timeout_record_list.insert({deadline_seconds, index});
    }
}

void TcpServer::OnCheckingEvent(time_t current) {

    // At least 1s to check once
    if (current - _last_timeout_time.Load() < 1) {
        return;
    }
    _last_timeout_time.Store(current);

    AutoMutex g(&_conn_mtx);

    auto it = _timeout_record_list.begin();
    while (it != _timeout_record_list.end()) {
        if (it->first > current) {
            break;
        }

        uint32_t index = it->second;

        ++it;

        _mgr[index].first->Shutdown(true);
        DeleteConnection(index);
    }
}

// internal::INotificationTransfer impl
void TcpServer::OnConnectionArrived(ConnectionId cid, const raptor_resolved_address* addr) {
    TcpMessageNode* msg = new TcpMessageNode;
    msg->cid = cid;
    msg->addr = *addr;
    msg->type = MessageType::kNewConnection;
    _mpscq.push(&msg->node);
    _count.FetchAdd(1, MemoryOrder::ACQ_REL);
    _cv.Signal();
}

void TcpServer::OnDataReceived(ConnectionId cid, const Slice* s) {
    TcpMessageNode* msg = new TcpMessageNode;
    msg->cid = cid;
    msg->slice = *s;
    msg->type = MessageType::kRecvAMessage;
    _mpscq.push(&msg->node);
    _count.FetchAdd(1, MemoryOrder::ACQ_REL);
    _cv.Signal();
}

void TcpServer::OnConnectionClosed(ConnectionId cid) {
    TcpMessageNode* msg = new TcpMessageNode;
    msg->cid = cid;
    msg->type = MessageType::kCloseClient;
    _mpscq.push(&msg->node);
    _count.FetchAdd(1, MemoryOrder::ACQ_REL);
    _cv.Signal();
}

void TcpServer::DeleteConnection(uint32_t index) {
    delete _mgr[index].first;
    _mgr[index].first = nullptr;
    _timeout_record_list.erase(_mgr[index].second);
    _mgr[index].second = _timeout_record_list.end();
    _free_index_list.push_back(index);
}

bool TcpServer::SetUserData(ConnectionId cid, void* ptr) {
    uint32_t index = CheckConnectionId(cid);
    if (index == InvalidIndex) {
        return false;
    }

    if (_mgr[index].first) {
        _mgr[index].first->SetUserData(ptr);
        return true;
    }
    return false;
}

bool TcpServer::GetUserData(ConnectionId cid, void** ptr) const {
    uint32_t index = CheckConnectionId(cid);
    if (index == InvalidIndex) {
        return false;
    }

    if (_mgr[index].first) {
        _mgr[index].first->GetUserData(ptr);
        return true;
    }
    return false;
}

bool TcpServer::SetExtendInfo(ConnectionId cid, uint64_t data) {
    uint32_t index = CheckConnectionId(cid);
    if (index == InvalidIndex) {
        return false;
    }

    if (_mgr[index].first) {
        _mgr[index].first->SetExtendInfo(data);
        return true;
    }
    return false;
}

bool TcpServer::GetExtendInfo(ConnectionId cid, uint64_t& data) const {
    uint32_t index = CheckConnectionId(cid);
    if (index == InvalidIndex) {
        return false;
    }

    if (_mgr[index].first) {
        _mgr[index].first->GetExtendInfo(data);
        return true;
    }
    return false;
}

uint32_t TcpServer::CheckConnectionId(ConnectionId cid) const {
    uint32_t failure = InvalidIndex;
    if (cid == core::InvalidConnectionId) {
        return failure;
    }
    uint16_t magic = core::GetMagicNumber(cid);
    if (magic != _magic_number) {
        return failure;
    }

    uint32_t index = core::GetUserId(cid);
    if (index >= _options.max_connections) {
        return failure;
    }
    return index;
}

void TcpServer::MessageQueueThread(void*) {
    while (!_shutdown) {
        RaptorMutexLock(_mutex);

        while (_count.Load() == 0) {
            _cv.Wait(&_mutex);
            if (_shutdown) {
                RaptorMutexUnlock(_mutex);
                return;
            }
        }
        auto n = _mpscq.pop();
        auto msg = reinterpret_cast<struct TcpMessageNode*>(n);

        if (msg != nullptr) {
            _count.FetchSub(1, MemoryOrder::RELAXED);
            this->Dispatch(msg);
            delete msg;
        }
        RaptorMutexUnlock(_mutex);
    }
}

void TcpServer::Dispatch(struct TcpMessageNode* msg) {
    switch (msg->type) {
    case MessageType::kNewConnection:
        _service->OnConnected(msg->cid);
        break;
    case MessageType::kRecvAMessage:
        _service->OnMessageReceived(msg->cid, msg->slice.begin(), msg->slice.size());
        break;
    case MessageType::kCloseClient:
        _service->OnClosed(msg->cid);
        break;
    default:
        log_error("unknow message type %d", static_cast<int>(msg->type));
        break;
    }
}

} // namespace raptor
