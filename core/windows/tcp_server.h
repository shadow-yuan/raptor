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
#pragma once
#include <memory>
#include <string>
#include <vector>

#include "core/acceptor.h"
#include "core/resolve_address.h"
#include "core/iocp/iocp.h"
#include "core/iocp/tcp_listener.h"
#include "core/cid.h"
#include "core/iocp/connection.h"
#include "core/sync.h"
#include "core/thread.h"
#include "core/status.h"
#include "raptor/notification.h"
#include "raptor/types.h"

namespace raptor {

class TcpServer : public IAcceptor, public internal::ITransferService {
public:
    TcpServer(IRaptorServerEvent *service);
    ~TcpServer();

    raptor_error Init(const RaptorOptions* options);
    raptor_error AddListening(const std::string& addr);
    raptor_error Start();
    raptor_error Shutdown();

    raptor_error Send(ConnectionId cid, const void* buf, size_t len);
    raptor_error CloseConnection(ConnectionId cid);

    // IAcceptor impl
    void OnNewConnection(
        SOCKET fd, int32_t listen_port,
        const raptor_resolved_address* addr) override;

    //ITransferService impl
    void OnMessage(ConnectionId cid, const Slice& s) override;
    void OnClose(ConnectionId cid) override;

    // user data
    raptor_error SetUserData(ConnectionId id, void* userdata);
    raptor_error GetUserData(ConnectionId id, void** userdata);

private:
    void WorkThread();

    /* data */
    bool _shutdown;
    Iocp _iocp;
    Mutex _mtx; // for _mgr
    std::vector<std::shared_ptr<Connection>> _mgr;
    std::shared_ptr<TcpListener> _listener;
    Thread _thd;
    OVERLAPPED _exit;
    IRaptorServerEvent* _service;
};
} // namespace raptor
