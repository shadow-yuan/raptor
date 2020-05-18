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

namespace raptor {
namespace {
enum MessageType {
    kNewConnection,
    kRecvAMessage,
    kCloseClient,
};
} // namespace
struct TcpMessageNode {
    MessageQueue::Node node;
    MessageType type;
    ConnectionId cid;
    raptor_resolved_address addr;
    Slice slice;
};

TcpServer::TcpServer(ITcpServerService* service)
    : _service(service), _shutdown(true) {}

TcpServer::~TcpServer() {}

RefCountedPtr<Status> TcpServer::Init() {
    if (!_shutdown) return RAPTOR_ERROR_NONE;

    _count.Store(0);
    _index.Store(0);
    _listener = std::make_shared<TcpListener>(this);
    _recv_thread = std::make_shared<RecvSendThread>(this);
    _send_thread = std::make_shared<RecvSendThread>(this);
    _protocol = std::make_shared<Protocol>();

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

    _thd = Thread("message_queue",
        [] (void* param) ->void {
            TcpServer* pthis = (TcpServer*)param;
            pthis->MessageQueueThread();
        },
        this);

    _shutdown = false;
    return RAPTOR_ERROR_NONE;
}

RefCountedPtr<Status> TcpServer::AddListeningPort(const char* addr) {
    if (_shutdown) return RAPTOR_ERROR_FROM_STATIC_STRING("tcp server uninitialized");
    if (!addr) return RAPTOR_ERROR_FROM_STATIC_STRING("invalid parameters");
    raptor_resolved_addresses* addrs;
    auto ret = p2p_blocking_resolve_address(addr, nullptr, &addrs);
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
    _thd.Start();
    return true;
}

bool TcpServer::Shutdown() {
    if (!_shutdown) {
        _shutdown = true;
        _cv.Signal();
        _listener->Shutdown();
        _recv_thread->Shutdown();
        _send_thread->Shutdown();
        _thd.Join();

        _conn_mtex.Lock();
        for (auto& obj : _conn_mgr) {
            if (obj) {
                obj->Shutdown(false);
            }
        }
        _conn_mgr.clear();
        _conn_mtex.Unlock();

        // clear message queue
        bool empty = true;
        do {
            auto n = _mq.PopAndCheckEnd(&empty);
            auto msg = reinterpret_cast<TcpMessageNode*>(n);
            if (msg != nullptr) {
                _count.FetchSub(1, MemoryOrder::Relaxed);
                delete msg;
            }
        } while (!empty);
        return true;
    }
    return false;
}



// Acceptor implement
void TcpServer::OnNewConnection(int sock_fd,
    int listen_port, const raptor_resolved_address* addr) {
#ifdef P2PCOMM_SSL
    std::shared_ptr<OpenSSL> ssl;
    if (!DoHandshake(sock_fd, ssl)) {
#else
    if (!DoHandshake(sock_fd)) {
#endif
        p2p_set_socket_shutdown(sock_fd);
        return;
    }
    AutoMutex g(&_conn_mtex);
    int position = -1;
    if (!_conn_mgr.empty()) {
        size_t floor = _index.Load();
        if (floor >= _conn_mgr.size() - 1) {
            floor = 0;
        }
        for (size_t i = floor; i < _conn_mgr.size(); i++) {
            if (!_conn_mgr[i]) {
                _conn_mgr[i] = std::make_shared<Connection>(this);
                position = static_cast<int>(i);
                break;
            }
            if (!_conn_mgr[i]->IsOnline()) {
                position = static_cast<int>(i);
                break;
            }
        }
    }

    ConnectionId cid;
    cid.s.fd = sock_fd;

    if (position == -1) {
        position = _conn_mgr.size();
        cid.s.index = position;
        _conn_mgr.push_back(std::make_shared<Connection>(this));
    } else {
        cid.s.index = position;
    }

    _index.Store(static_cast<size_t>(position));

    std::shared_ptr<Connection> conn = _conn_mgr[position];
    conn->Init(cid, addr,
        _recv_thread.get(), _send_thread.get()
#ifdef P2PCOMM_SSL
        , ssl
#endif
    );

    OnNewConnection(cid, addr);
}

// Receiver implement (epoll event)
void TcpServer::OnError(void* ptr) {
    AutoMutex g(&_conn_mtex);
    ConnectionId cid = ConvertConnectionId((uint64_t)ptr);
    if (cid.s.index < 0 || cid.s.index >= (int)_conn_mgr.size()) {
        log_error("TcpServer: invalid internal index %d, fd = %d", cid.s.index, cid.s.fd);
        return;
    }

    if (_conn_mgr[cid.s.index]) {
        _conn_mgr[cid.s.index]->Shutdown(true);
    }
}

void TcpServer::OnRecvEvent(void* ptr) {
    AutoMutex g(&_conn_mtex);
    ConnectionId cid = ConvertConnectionId((uint64_t)ptr);
    if (cid.s.index < 0 || cid.s.index >= (int)_conn_mgr.size()) {
        log_error("TcpServer: invalid internal index %d, fd = %d", cid.s.index, cid.s.fd);
        return;
    }

    if (_conn_mgr[cid.s.index]) {
        _conn_mgr[cid.s.index]->DoRecvEvent();
    }
}

void TcpServer::OnSendEvent(void* ptr) {
    AutoMutex g(&_conn_mtex);
    ConnectionId cid = ConvertConnectionId((uint64_t)ptr);
    if (cid.s.index < 0 || cid.s.index >= (int)_conn_mgr.size()) {
        log_error("TcpServer: invalid internal index %d, fd = %d", cid.s.index, cid.s.fd);
        return;
    }
    if (_conn_mgr[cid.s.index]) {
        _conn_mgr[cid.s.index]->DoSendEvent();
    }
}

// ServiceInterface implement
void TcpServer::OnNewConnection(ConnectionId cid, const raptor_resolved_address* addr) {
    TcpMessageNode* msg = new TcpMessageNode;
    msg->cid = cid;
    msg->addr = *addr;
    msg->type = MessageType::kNewConnection;
    _mq.push(&msg->node);
    _count.FetchAdd(1, MemoryOrder::Acq_Rel);
    _cv.Signal();
}

void TcpServer::OnMessage(ConnectionId cid, const Slice* s) {
    TcpMessageNode* msg = new TcpMessageNode;
    msg->cid = cid;
    msg->slice = *s;
    msg->type = MessageType::kRecvAMessage;
    _mq.push(&msg->node);
    _count.FetchAdd(1, MemoryOrder::Acq_Rel);
    _cv.Signal();
}

void TcpServer::OnClose(ConnectionId cid) {
    TcpMessageNode* msg = new TcpMessageNode;
    msg->cid = cid;
    msg->type = MessageType::kCloseClient;
    _mq.push(&msg->node);
    _count.FetchAdd(1, MemoryOrder::Acq_Rel);
    _cv.Signal();
}

// send data
RefCountedPtr<Status> TcpServer::GeneralSend(ConnectionId cid, const void* buf, size_t len) {
    AutoMutex g(&_conn_mtex);
    if (cid.s.index < 0 || cid.s.index >= (int)_conn_mgr.size()) {
        return RAPTOR_ERROR_FROM_STATIC_STRING("invalid cid");
    }
    if (!_conn_mgr[cid.s.index]) {
        return RAPTOR_ERROR_FROM_STATIC_STRING("connection not exists");
    }
    if (!_conn_mgr[cid.s.index]->IsOnline()) {
        return RAPTOR_ERROR_FROM_STATIC_STRING("connection not online");
    }

    Slice hd = _protocol->BuildPackageHeader(len);
    _conn_mgr[cid.s.index]->Send(hd, buf, len);
    return RAPTOR_ERROR_NONE;
}

RefCountedPtr<Status> TcpServer::ChannelSend(uint32_t channel_id, ConnectionId cid, const void* buf, size_t len) {
    if (!_channel_mgr.Exist(channel_id)) {
        return RAPTOR_ERROR_FROM_STATIC_STRING("channel not exists");
    }

    AutoMutex g(&_conn_mtex);
    if (cid.s.index < 0 || cid.s.index >= (int)_conn_mgr.size()) {
        return RAPTOR_ERROR_FROM_STATIC_STRING("invalid cid");
    }
    if (!_conn_mgr[cid.s.index]) {
        return RAPTOR_ERROR_FROM_STATIC_STRING("connection not exists");
    }
    if (!_conn_mgr[cid.s.index]->IsOnline()) {
        return RAPTOR_ERROR_FROM_STATIC_STRING("connection not online");
    }

    Slice hd = _protocol->BuildPackageHeader(len, channel_id);
    _conn_mgr[cid.s.index]->Send(hd, buf, len);
    return RAPTOR_ERROR_NONE;
}

void TcpServer::CloseConnection(ConnectionId cid) {
    AutoMutex g(&_conn_mtex);
    if (cid.s.index < 0 || cid.s.index >= (int)_conn_mgr.size()) {
        return;
    }
    if (!_conn_mgr[cid.s.index]) {
        return;
    }
    if (_conn_mgr[cid.s.index]->IsOnline()) {
        _conn_mgr[cid.s.index]->Shutdown(false);
    }
    _conn_mgr[cid.s.index].reset();
}

void TcpServer::MessageQueueThread() {
    for (; !_shutdown; ) {
        p2p_mutex_lock(_mutex.get());

        while (_count.Load() == 0) {
            _cv.Wait(&_mutex);
            if (_shutdown) {
                p2p_mutex_unlock(_mutex.get());
                return;
            }
        }
        auto n = _mq.pop();
        TcpMessageNode* msg = reinterpret_cast<TcpMessageNode*>(n);

        if (msg != nullptr) {
            _count.FetchSub(1, MemoryOrder::Relaxed);
            this->Dispatch(msg);
            delete msg;
        }
        p2p_mutex_unlock(_mutex.get());
    }
}

void TcpServer::Dispatch(struct TcpMessageNode* msg) {
    switch (msg->type) {
    case MessageType::kNewConnection:
        _service->OnNewConnection(msg->cid, &msg->addr);
        break;
    case MessageType::kRecvAMessage:
        ChannelDispatch(msg->cid, msg->slice);
        break;
    case MessageType::kCloseClient:
        // When receiving data error, we shutdown connection
        // and remove it from _conn_mgr here
        CloseConnection(msg->cid);
        _service->OnClose(msg->cid);
        break;
    default:
        log_error("unknow message type %d", static_cast<int>(msg->type));
        break;
    }
}

RefCountedPtr<Status> TcpServer::SetUserData(ConnectionId cid, void* ptr) {
    AutoMutex g(&_conn_mtex);
    if (cid.s.index < 0 || cid.s.index >= (int)_conn_mgr.size()) {
        return RAPTOR_ERROR_FROM_STATIC_STRING("ConnectionId not exist.");
    }
    if (_conn_mgr[cid.s.index]) {
        _conn_mgr[cid.s.index]->SetUserData(ptr);
        return RAPTOR_ERROR_NONE;
    }
    return RAPTOR_ERROR_FROM_STATIC_STRING("Connection object is null.");
}

void* TcpServer::GetUserData(ConnectionId cid) {
    AutoMutex g(&_conn_mtex);
    if (cid.s.index < 0 || cid.s.index >= (int)_conn_mgr.size()) {
        return nullptr;
    }
    if (_conn_mgr[cid.s.index]) {
        return _conn_mgr[cid.s.index]->GetUserData();
    }
    return nullptr;
}

}  // namespace raptor
