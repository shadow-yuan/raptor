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
#include "core/sockaddr.h"
#include "core/windows/iocp.h"
#include "core/windows/service.h"
#include "util/list_entry.h"
#include "util/sync.h"
#include "util/thread.h"

namespace raptor {
struct tcp_listener;
class TcpListener final {
public:
    explicit TcpListener(IAcceptor* service);
    ~TcpListener();

    bool Init();
    raptor_error AddListeningPort(const raptor_resolved_address* addr);
    bool Start();
    bool Shutdown();

private:
    void WorkThread();
    raptor_error StartAcceptEx(struct tcp_listener*);
    void ParsingNewConnectionAddress(
        const tcp_listener* sp, raptor_resolved_address* remote);

    raptor_error GetExtensionFunction(SOCKET fd);

    IAcceptor* _service;
    bool _shutdown;
    list_entry _head;
    Iocp _iocp;
    Thread _thd[2];
    raptor_mutex_t _mutex;
    OVERLAPPED _exit;
    LPFN_ACCEPTEX _AcceptEx;
    LPFN_GETACCEPTEXSOCKADDRS _GetAcceptExSockAddrs;
};
} // namespace raptor
