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

#ifndef __RAPTOR_CORE_WINDOWS_TCP_LISTENER__
#define __RAPTOR_CORE_WINDOWS_TCP_LISTENER__

#include "core/windows/iocp.h"
#include "core/service.h"
#include "core/sockaddr.h"
#include "util/list_entry.h"
#include "util/sync.h"
#include "util/thread.h"

namespace raptor {

struct ListenerObject;

class TcpListener final {
public:
    explicit TcpListener(internal::IAcceptor* service);
    ~TcpListener();

    raptor_error Init(int max_threads = 1);
    raptor_error AddListeningPort(const raptor_resolved_address* addr);
    bool Start();
    void Shutdown();

private:
    void WorkThread(void* ptr);
    raptor_error StartAcceptEx(struct ListenerObject*);
    void ParsingNewConnectionAddress(
        const ListenerObject* sp, raptor_resolved_address* remote);

    raptor_error GetExtensionFunction(SOCKET fd);

    internal::IAcceptor* _service;
    bool _shutdown;
    Thread* _threads;
    list_entry _head;
    Iocp _iocp;
    raptor_mutex_t _mutex;  // for list_entry
    OVERLAPPED _exit;
    LPFN_ACCEPTEX _AcceptEx;
    LPFN_GETACCEPTEXSOCKADDRS _GetAcceptExSockAddrs;
    int _max_threads;
};

} // namespace raptor

#endif  // __RAPTOR_CORE_WINDOWS_TCP_LISTENER__
