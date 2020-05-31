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

#include "raptor/client.h"
#ifdef _WIN32
#include "core/windows/tcp_client.h"
#else
#include "core/linux/tcp_client.h"
#endif

#include "util/log.h"
#include "util/status.h"

namespace raptor {
Client::Client(IClientReceiver* service) {
    _impl = new TcpClient(service);
}

Client::~Client() {
    delete _impl;
}

bool Client::Init() {
    raptor_error e = _impl->Init();
    if (e != RAPTOR_ERROR_NONE) {
        log_error("client: init (%s)", e->ToString().c_str());
        return false;
    }
    return true;
}

void Client::SetProtocol(IProtocol* proto) {
    _impl->SetProtocol(proto);
}

bool Client::Connect(const char* addr, size_t timeout_ms) {
    raptor_error e = _impl->Connect(addr, timeout_ms);
    if (e != RAPTOR_ERROR_NONE) {
        log_error("client: connect (%s)", e->ToString().c_str());
        return false;
    }
    return true;
}

bool Client::Send(const void* buff, size_t len) {
    return _impl->Send(buff, len);
}

void Client::Shutdown() {
    _impl->Shutdown();
}

} // namespace raptor

raptor::ITcpClient* RaptorCreateClient(raptor::IClientReceiver* c) {
    if (!c) return nullptr;
    return new raptor::Client(c);
}

void RaptorReleaseClient(raptor::ITcpClient* client) {
    if (client) delete client;
}
