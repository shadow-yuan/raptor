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

#include "core/linux/tcp_server.h"
#include "core/linux/tcp_listen.h"
#include "core/socket_util.h"
#include "core/resolve_address.h"
#include "core/mpscq.h"
#include "util/time.h"
#include "core/linux/socket_setting.h"
#include "util/log.h"

namespace raptor {
namespace {
enum MessageType {
    kNewConnection,
    kRecvAMessage,
    kCloseClient,
};
} // namespace
struct TcpMessageNode {
    MultiProducerSingleConsumerQueue::Node node;
    MessageType type;
    ConnectionId cid;
    raptor_resolved_address addr;
    Slice slice;
};
constexpr uint32_t InvalidUID = static_cast<uint32_t>(-1);
TcpServer::TcpServer(IRaptorServerService* service)
    : _service(service), _shutdown(true) {}

TcpServer::~TcpServer() {}

RefCountedPtr<Status> TcpServer::Init(const RaptorOptions& options) {
    if (!_shutdown) return RAPTOR_ERROR_NONE;

    _options = options;
    _count.Store(0);

    _listener = std::make_shared<TcpListener>(this);
    _recv_thread = std::make_shared<SendRecvThread>(this);
    _send_thread = std::make_shared<SendRecvThread>(this);
    _proto = std::make_shared<Protocol>();

    auto e = _listener->Init();
    if (e != RAPTOR_ERROR_NONE) {
        return e;
    }

    e = _recv_thread->Init();
    if (e != RAPTOR_ERROR_NONE) {
        return e;
    }

    e = _send_thread->Init();
    if (e != RAPTOR_ERROR_NONE) {
        return e;
    }

    _mq_thread = Thread("message_queue",
        [] (void* param) ->void {
            TcpServer* pthis = (TcpServer*)param;
            pthis->MessageQueueThread();
        },
        this);

    _timeout_thread = Thread("timeout_check",
        [] (void* param) ->void {
            TcpServer* pthis = (TcpServer*)param;
            pthis->TimeoutCheckoutThread();
        },
        this);

    // init connection container
    _last_timeout_time = Now();
    _magic_number = _last_timeout_time & 0xffff;
    _current_index.Store(0);
    if (_options.max_connections < 100) {
        _options.max_connections = 100;
    }
    _conn_mgr.resize(100);
    for (size_t i = 0; i < _conn_mgr.size(); i++) {
        _conn_mgr[i].first = nullptr;
        _free_list.push_back(i);
    }
    _timeout_record.clear();

    _shutdown = false;
    return RAPTOR_ERROR_NONE;
}

RefCountedPtr<Status> TcpServer::AddListeningPort(const char* addr) {
    if (_shutdown) return RAPTOR_ERROR_FROM_STATIC_STRING("tcp server uninitialized");
    if (!addr) return RAPTOR_ERROR_FROM_STATIC_STRING("invalid parameters");
    raptor_resolved_addresses* addrs;
    auto ret = raptor_blocking_resolve_address(addr, nullptr, &addrs);
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

bool TcpServer::Start() {
    if (!_listener->StartListening()) {
        return false;
    }
    if (!_recv_thread->Start()) {
        return false;
    }

    if (!_send_thread->Start()) {
        return false;
    }
    _mq_thread.Start();
    _timeout_thread.Start();
    return true;
}

bool TcpServer::Shutdown() {
    if (!_shutdown) {
        _shutdown = true;
        _cv.Signal();
        _listener->Shutdown();
        _recv_thread->Shutdown();
        _send_thread->Shutdown();
        _mq_thread.Join();
        _timeout_thread.Join();

        _conn_mtx.Lock();
        for (auto& obj : _conn_mgr) {
            if (obj.first) {
                obj.first->Close(false);
            }
        }
        _conn_mgr.clear();
        _conn_mtx.Unlock();

        // clear message queue
        bool empty = true;
        do {
            auto n = _mq.PopAndCheckEnd(&empty);
            auto msg = reinterpret_cast<TcpMessageNode*>(n);
            if (msg != nullptr) {
                _count.FetchSub(1, MemoryOrder::RELAXED);
                delete msg;
            }
        } while (!empty);
        return true;
    }
    return false;
}

uint32_t TcpServer::GetIdlePosition() {
    AutoMutex g(&_conn_mtx);
    uint32_t index = 0;
    if (_free_list.empty()) {
        size_t count = _conn_mgr.size();
        if (count == _options.max_connections) {
            return static_cast<uint32_t>(-1);
        }
        size_t half = _options.max_connections / 2;
        size_t new_size = (count >= half) ? _options.max_connections : half;
        _conn_mgr.resize(new_size);
        for (size_t i = count; i < new_size; i++) {
            _conn_mgr[i].first = nullptr;
            _free_list.push_back(i);
        }
    }
    index = _free_list.front();
    _free_list.pop_front();
    return index;
}

// IAcceptor implement
void TcpServer::OnNewConnection(int sock_fd,
    int listen_port, const raptor_resolved_address* addr) {

    uint32_t index = GetIdlePosition();
    if (index == static_cast<uint32_t>(-1)) {
        raptor_set_socket_shutdown(sock_fd);
        log_error("The number of connections has reached the maximum: %u", _options.max_connections);
        return;
    }

    time_t timeout_second = Now() + _options.connection_timeout;
    ConnectionId cid = core::BuildConnectionId(_magic_number, (uint16_t)listen_port, index);
    std::multimap<time_t, uint32_t>::iterator iter = _timeout_record.insert({timeout_second, index});

    if (!_conn_mgr[index].first) {
        _conn_mgr[index].first = new Connection(this);
        _conn_mgr[index].first->Init(cid, sock_fd, addr, _recv_thread.get(), _send_thread.get());
        _conn_mgr[index].first->SetProtocol(_proto.get());
    } else {
        _conn_mgr[index].first->Init(cid, sock_fd, addr, _recv_thread.get(), _send_thread.get());
    }
    _conn_mgr[index].second = iter;
}

// Receiver implement (epoll event)
void TcpServer::OnError(void* ptr) {
    ConnectionId cid = (ConnectionId)ptr;
    uint32_t index = core::GetUserId(cid);
    _conn_mgr[index].first->Close(true);
}

void TcpServer::OnRecvEvent(void* ptr) {
    ConnectionId cid = (ConnectionId)ptr;
    uint32_t index = core::GetUserId(cid);
    _conn_mgr[index].first->DoRecvEvent();
}

void TcpServer::OnSendEvent(void* ptr) {
    ConnectionId cid = (ConnectionId)ptr;
    uint32_t index = core::GetUserId(cid);
    _conn_mgr[index].first->DoSendEvent();

}

// ServiceInterface implement
void TcpServer::OnConnectionArrived(ConnectionId cid, const raptor_resolved_address* addr) {
    TcpMessageNode* msg = new TcpMessageNode;
    msg->cid = cid;
    msg->addr = *addr;
    msg->type = MessageType::kNewConnection;
    _mq.push(&msg->node);
    _count.FetchAdd(1, MemoryOrder::ACQ_REL);
    _cv.Signal();
}

void TcpServer::OnDataReceived(ConnectionId cid, const Slice* s) {
    TcpMessageNode* msg = new TcpMessageNode;
    msg->cid = cid;
    msg->slice = *s;
    msg->type = MessageType::kRecvAMessage;
    _mq.push(&msg->node);
    _count.FetchAdd(1, MemoryOrder::ACQ_REL);
    _cv.Signal();
}

void TcpServer::OnConnectionClosed(ConnectionId cid) {
    TcpMessageNode* msg = new TcpMessageNode;
    msg->cid = cid;
    msg->type = MessageType::kCloseClient;
    _mq.push(&msg->node);
    _count.FetchAdd(1, MemoryOrder::ACQ_REL);
    _cv.Signal();
}

// send data
RefCountedPtr<Status> TcpServer::SendData(ConnectionId cid, const void* buf, size_t len) {
    if (cid == core::InvalidConnectionId) {
        return RAPTOR_ERROR_FROM_STATIC_STRING("invalid cid");
    }
    uint16_t magic = core::GetMagicNumber(cid);
    if (magic != _magic_number) {
        return RAPTOR_ERROR_FROM_STATIC_STRING("invalid cid: magic number error");
    }
    uint32_t index = core::GetUserId(cid);
    if (!_conn_mgr[index].first) {
        return RAPTOR_ERROR_FROM_STATIC_STRING("connection not exists");
    }
    if (!_conn_mgr[index].first->IsOnline()) {
        return RAPTOR_ERROR_FROM_STATIC_STRING("connection not online");
    }

    Slice hd = _proto->BuildPackageHeader(len);
    _conn_mgr[index].first->Send(hd, buf, len);
    return RAPTOR_ERROR_NONE;
}

void TcpServer::CloseConnection(ConnectionId cid) {
    if (cid == core::InvalidConnectionId) {
        return;
    }
    uint16_t magic = core::GetMagicNumber(cid);
    if (magic != _magic_number) {
        return;
    }
    uint32_t index = core::GetUserId(cid);
    if (!_conn_mgr[index].first) {
        return;
    }
    if (!_conn_mgr[index].first->IsOnline()) {
        return;
    }

    _conn_mgr[index].first->Close(false);
}

void TcpServer::MessageQueueThread() {
    while (!_shutdown) {
        RaptorMutexLock(_mutex);

        while (_count.Load() == 0) {
            _cv.Wait(&_mutex);
            if (_shutdown) {
                RaptorMutexUnlock(_mutex);
                return;
            }
        }
        auto n = _mq.pop();
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
        ChannelDispatch(msg->cid, msg->slice);
        break;
    case MessageType::kCloseClient:
        _service->OnClosed(msg->cid);
        break;
    default:
        log_error("unknow message type %d", static_cast<int>(msg->type));
        break;
    }
}

bool TcpServer::SetUserData(ConnectionId cid, void* ptr) {
    uint32_t uid = CheckConnectionId(cid);
    if (uid == InvalidUID) {
        return false;
    }

    _conn_mgr[uid].first->SetUserData(ptr);
    return true;
}

bool TcpServer::GetUserData(ConnectionId cid, void** ptr) const {
    uint32_t uid = CheckConnectionId(cid);
    if (uid == InvalidUID) {
        return false;
    }

    _conn_mgr[uid].first->GetUserData(ptr);
    return true;
}

bool TcpServer::SetExtendInfo(ConnectionId cid, uint64_t data) {
    uint32_t uid = CheckConnectionId(cid);
    if (uid == InvalidUID) {
        return false;
    }

    _conn_mgr[uid].first->SetExtendInfo(data);
    return true;
}

bool TcpServer::GetExtendInfo(ConnectionId cid, uint64_t& data) const {
    uint32_t uid = CheckConnectionId(cid);
    if (uid == InvalidUID) {
        return false;
    }

    _conn_mgr[uid].first->GetExtendInfo(data);
    return true;
}

uint32_t TcpServer::CheckConnectionId(ConnectionId cid) const {
    uint32_t failure = InvalidUID;
    if (cid == core::InvalidConnectionId) {
        return failure;
    }
    uint16_t magic = core::GetMagicNumber(cid);
    if (magic != _magic_number) {
        return failure;
    }

    uint16_t uid = core::GetUserId(cid);
    if (uid >= _options.max_connections) {
        return failure;
    }
    return uid;
}

}  // namespace raptor
