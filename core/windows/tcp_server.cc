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
#include "util/alloc.h"
#include "util/cpu.h"

namespace raptor {

TcpServer::TcpServer(IRaptorServerMessage *service)
    : _service(service)
    , _shutdown(true)
    , _rs_threads(nullptr) {
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

    _options = *options;

    size_t kernel_threads = 0;
    size_t cpu_cores = (size_t)raptor_get_number_of_cpu_cores();

    if (_options.send_recv_threads == 0) {  // default
        _options.send_recv_threads = 1;
        kernel_threads = 2;
    } else if (_options.send_recv_threads > cpu_cores) {
        _options.send_recv_threads = cpu_cores;
        kernel_threads = 0;
    } else {
        kernel_threads = _options.send_recv_threads;
    }

    if (!_iocp.create(kernel_threads)) {
        return RAPTOR_WINDOWS_ERROR(WSAGetLastError(), "CreateIoCompletionPort");
    }

    if (_options.accept_threads > cpu_cores) {
        _options.accept_threads = cpu_cores / 2;
    }
    if (_options.accept_threads == 0) {
        _options.accept_threads = 1;
    }

    _listener = std::make_shared<TcpListener>(this);
    auto e = _listener->Init(_options.accept_threads);
    if (e != RAPTOR_ERROR_NONE) {
        return e;
    }

    _shutdown = false;

    InitConnectionList();
    InitTimeoutThread();
    InitWorkThread();

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
    _timeout_threads.Start();
    for (uint16_t i = 0; i < _options.send_recv_threads; i++) {
        _rs_threads[i].Start();
    }
    return RAPTOR_ERROR_NONE;
}

void TcpServer::Shutdown(){
    if (!_shutdown) {
        _shutdown = true;
        _iocp.post(NULL, &_exit);
        _timeout_threads.Join();
        for (uint16_t i = 0; i < _options.send_recv_threads; i++) {
            _rs_threads[i].Join();
        }
    }
}

bool TcpServer::Send(ConnectionId cid, const void* buf, size_t len) {
    if (cid == core::InvalidConnectionId) {
        return false;
    }

    auto conn = _conn_list->GetConnection(cid);
    if (conn) {
        conn->Send(buf, len);
    }
    return true;
}

bool TcpServer::CloseConnection(ConnectionId cid){
    if (cid == core::InvalidConnectionId) {
        return false;
    }

    auto conn = _conn_list->GetConnection(cid);
    if (conn) {
        conn->Shutdown(false);
        _conn_list->Remove(cid);
    }
    return true;
}

// internal::IAcceptor impl
void TcpServer::OnNewConnection(
    raptor_socket_t sock, int listen_port,
    const raptor_resolved_address* addr) {
    time_t timeout_time = Now() + _options.connection_timeout;
    std::unique_ptr<Connection> conn(new Connection(this));
    uint32_t uid = _conn_list->GetUniqueId();
    uint16_t magic = _conn_list->GetMagicNumber();
    ConnectionId cid = core::BuildConnectionId(magic, listen_port, uid);
    conn->Init(cid, sock, addr);
    _conn_list->Add(conn.release(), timeout_time);
}

// IRaptorServerMessage impl
void TcpServer::OnMessage(ConnectionId cid, const Slice* s){

}

void TcpServer::OnClose(ConnectionId cid) {

}


// IRaptorServerMessage impl
void TcpServer::OnConnected(ConnectionId cid) {

}

void TcpServer::OnMessageReceived(ConnectionId cid, const void* s, size_t len) {

}

void TcpServer::OnClosed(ConnectionId cid) {

}

void TcpServer::InitTimeoutThread() {
    _timeout_threads = Thread("timeout-check",
        [](void* param) ->void {
            TcpServer* p = (TcpServer*)param;
            p->TimeoutCheckThread();
        },
        this);
}

void TcpServer::InitWorkThread() {
    _rs_threads = new Thread[_options.send_recv_threads];
    for (uint16_t i = 0; i < _options.send_recv_threads; i++) {
        _rs_threads[i] = Thread("rs-thread",
            [](void* param) ->void {
                TcpServer* p = (TcpServer*)param;
                p->WorkThread();
            },
            this);
    }
}

void TcpServer::WorkThread(){
    while (!_shutdown) {

    }
}

void TcpServer::SetUserData(ConnectionId cid, void* userdata) {
    auto conn = _conn_list->GetConnection(cid);
    if (conn) {
        conn->SetUserData(userdata, userdata);
    }
}

void* TcpServer::GetUserData(ConnectionId cid) {
    auto conn = _conn_list->GetConnection(cid);
    if (conn) {
        return conn->GetUserData(userdata);
    }
    return nullptr;
}

} // namespace raptor
