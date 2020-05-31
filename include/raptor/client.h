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

#ifndef __RAPTOR_EXPORT_CLIENT__
#define __RAPTOR_EXPORT_CLIENT__

#include "raptor/export.h"
#include "raptor/protocol.h"
#include "raptor/service.h"

namespace raptor {

class TcpClient;

class Client final : public ITcpClient {
public:
    explicit Client(IClientReceiver* service);
    ~Client();

    Client(const Client&) = delete;
    Client& operator= (const Client&) = delete;

    bool Init() override;
    void SetProtocol(IProtocol* proto) override;
    bool Connect(const char* addr, size_t timeout_ms) override;
    bool Send(const void* buff, size_t len) override;
    void Shutdown() override;

private:
    TcpClient* _impl;
};

} // namespace raptor

RAPTOR_API raptor::ITcpClient* RaptorCreateClient(raptor::IClientReceiver* c);
RAPTOR_API void RaptorReleaseClient(raptor::ITcpClient* client);

#endif  // __RAPTOR_EXPORT_CLIENT__
