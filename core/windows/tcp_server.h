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
#include <stdint.h>
#include <memory>
#include <string>

#include "core/resolve_address.h"
#include "core/windows/iocp.h"
#include "core/windows/tcp_listener.h"
#include "core/cid.h"
#include "core/connection_list.h"
#include "core/windows/connection.h"
#include "core/mpscq.h"

#include "util/sync.h"
#include "util/thread.h"
#include "util/status.h"
#include "raptor/service.h"

namespace raptor {
class Slice;
class TcpListener;

class TcpServer : public internal::IAcceptor
                , public internal::IMessageTransfer
                , public IRaptorServerMessage {
public:
    TcpServer(IRaptorServerMessage *service);
    ~TcpServer();

    raptor_error Init(const RaptorOptions* options);
    raptor_error AddListening(const std::string& addr);
    raptor_error Start();
    void Shutdown();

    bool Send(ConnectionId cid, const void* buf, size_t len);
    bool CloseConnection(ConnectionId cid);

    // internal::IAcceptor impl
    void OnNewConnection(
        raptor_socket_t sock, int listen_port,
        const raptor_resolved_address* addr) override;

    // internal::IMessageTransfer impl
    void OnMessage(ConnectionId cid, const Slice* s) override;
    void OnClose(ConnectionId cid) override;

    // IRaptorServerMessage impl
    void OnConnected(ConnectionId cid) override;
    void OnMessageReceived(ConnectionId cid, const void* s, size_t len) override;
    void OnClosed(ConnectionId cid) override;

    // user data
    void SetUserData(ConnectionId id, void* userdata);
    void* GetUserData(ConnectionId id);

private:
    void WorkThread();
    void TimeoutCheckThread();

    void InitConnectionList();
    void InitTimeoutThread();
    void InitWorkThread();

    IRaptorServerMessage* _service;
    bool _shutdown;
    Thread* _rs_threads;
    RaptorOptions _options;
    Iocp _iocp;
    std::shared_ptr<TcpListener> _listener;
    std::shared_ptr<ConnectionList> _conn_list;
    MultiProducerSingleConsumerQueue _mpscq;
    Thread _timeout_threads;
    OVERLAPPED _exit;
};
} // namespace raptor
