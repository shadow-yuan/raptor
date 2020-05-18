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
#include "core/service.h"
#include "core/resolve_address.h"
#include "core/linux/epoll.h"
#include "util/list_entry.h"
#include "util/status.h"
#include "util/sync.h"
#include "util/thread.h"

namespace raptor {
struct tcp_listener;
class TcpListener final {
public:
    explicit TcpListener(Acceptor* cp);
    ~TcpListener();

    RefCountedPtr<Status>
        Init();
    RefCountedPtr<Status>
        AddListeningPort(const p2p_resolved_address* addr);
    bool StartListening();
    void Shutdown();

private:
    void DoPolling();
    void ProcessEpollEvents(void* ptr, uint32_t events);

    Thread _thd;
    poller _epoll;
    bool _shutdown;
    Acceptor* _acceptor; //not owned it
    list_entry _head;
    Mutex _mtex;
};

} // namespace raptor
