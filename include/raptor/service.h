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

#ifndef __RAPTOR_EXPORT_SERVICE__
#define __RAPTOR_EXPORT_SERVICE__

#include "raptor/export.h"
#include "raptor/types.h"

namespace raptor {
class IProtocol;
class IServerReceiver {
public:
    virtual ~IServerReceiver() {}
    virtual void OnConnected(ConnectionId cid) = 0;
    virtual void OnMessageReceived(ConnectionId cid, const void* s, size_t len) = 0;
    virtual void OnClosed(ConnectionId cid) = 0;
};

class RAPTOR_API ITcpServer {
public:
    virtual ~ITcpServer() {}
    virtual bool Init(const RaptorOptions* options) = 0;
    virtual void SetProtocol(IProtocol* proto) = 0;
    virtual bool AddListening(const char* addr) = 0;
    virtual bool Start() = 0;
    virtual void Shutdown() = 0;
    virtual bool Send(ConnectionId cid, const void* buff, size_t len) = 0;
    virtual bool CloseConnection(ConnectionId cid) = 0;
};

class IClientReceiver {
public:
    virtual ~IClientReceiver() {}
    virtual void OnConnectResult(bool success) = 0;
    virtual void OnMessageReceived(const void* s, size_t len) = 0;
    virtual void OnClosed() = 0;
};

class RAPTOR_API ITcpClient {
public:
    virtual ~ITcpClient() {}
    virtual bool Init() = 0;
    virtual void SetProtocol(IProtocol* proto) = 0;
    virtual bool Connect(const char* addr, size_t timeout_ms) = 0;
    virtual bool Send(const void* buff, size_t len) = 0;
    virtual void Shutdown() = 0;
};
}

#endif  // __RAPTOR_EXPORT_SERVICE__
