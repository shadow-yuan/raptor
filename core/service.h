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

#ifndef __RAPTOR_CORE_SERVICE__
#define __RAPTOR_CORE_SERVICE__

#include "core/cid.h"
#include "core/sockaddr.h"
#include "core/resolve_address.h"

namespace raptor {

class Slice;

// accepte
class IAcceptor {
public:
    virtual ~IAcceptor() {}
    virtual void OnNewConnection(
        raptor_socket_t rs, int listen_port, const raptor_resolved_address* addr) = 0;
};

// for epoll
class IReceiver {
public:
    virtual ~IReceiver() {}
    virtual void OnError(void* ptr) = 0;
    virtual void OnRecvEvent(void* ptr) = 0;
    virtual void OnSendEvent(void* ptr) = 0;
};

// async connect
class IConnectResult {
public:
    virtual ~IConnectResult() {}
    virtual void OnConnectResult(void* ptr, bool success) = 0;
};

class IMessageTransfer {
public:
    virtual ~IMessageTransfer() {}
    virtual void OnMessage(ConnectionId cid, const Slice* s) = 0;
    virtual void OnClose(ConnectionId cid) = 0;
};

} // namespace raptor

#endif // __RAPTOR_CORE_SERVICE__