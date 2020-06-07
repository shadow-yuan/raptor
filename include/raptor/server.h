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

#ifndef __RAPTOR_EXPORT_SERVER__
#define __RAPTOR_EXPORT_SERVER__

#include "raptor/export.h"
#include "raptor/protocol.h"
#include "raptor/service.h"

namespace raptor {
class TcpServer;
class RAPTOR_API Server : public ITcpServer {
public:
    explicit Server(IServerReceiver* service);
    ~Server();

    Server(const Server&) = delete;
    Server& operator= (const Server&) = delete;

    bool Init(const RaptorOptions* options) override;
    void SetProtocol(IProtocol* proto) override;
    bool AddListening(const char* addr) override;
    bool Start() override;
    void Shutdown() override;
    bool Send(ConnectionId cid, const void* buff, size_t len) override;
    bool SendWithHeader(ConnectionId cid,
        const void* hdr, size_t hdr_len, const void* data, size_t data_len) override;
    bool CloseConnection(ConnectionId cid) override;

    // user data
    bool SetUserData(ConnectionId id, void* userdatag);
    bool GetUserData(ConnectionId id, void** userdata);
    bool SetExtendInfo(ConnectionId id, uint64_t info);
    bool GetExtendInfo(ConnectionId id, uint64_t* info);

private:
    TcpServer* _impl;
};

} // namespace raptor

RAPTOR_API raptor::ITcpServer* RaptorCreateServer(raptor::IServerReceiver* s);
RAPTOR_API void RaptorReleaseServer(raptor::ITcpServer* server);

#endif  // __RAPTOR_EXPORT_SERVER__
