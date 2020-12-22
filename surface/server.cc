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

#include "raptor/server.h"

#ifdef _WIN32
#include "core/windows/tcp_server.h"
#else
#include "core/linux/tcp_server.h"
#endif

#include "util/log.h"
#include "util/status.h"

namespace raptor {
Server::Server(IServerReceiver* service) {
    _impl = new TcpServer(service);
}

Server::~Server() {
    delete _impl;
}

bool Server::Init(const RaptorOptions* options) {
    raptor_error e = _impl->Init(options);
    if (e != RAPTOR_ERROR_NONE) {
        log_error("server: init (%s)", e->ToString().c_str());
        return false;
    }

    return true;
}

void Server::SetProtocol(IProtocol* proto) {
    _impl->SetProtocol(proto);
}

bool Server::AddListening(const char* addr) {
    if (!addr) {
        log_error("server: invalid listening addr");
        return false;
    }

    raptor_error e = _impl->AddListening(addr);
    if (e != RAPTOR_ERROR_NONE) {
        log_error("server: add listening (%s)", e->ToString().c_str());
        return false;
    }

    return true;
}

bool Server::Start() {
    raptor_error e = _impl->Start();
    if (e != RAPTOR_ERROR_NONE) {
        log_error("server: start (%s)", e->ToString().c_str());
        return false;
    }

    return true;
}

void Server::Shutdown() {
    _impl->Shutdown();
}

bool Server::Send(ConnectionId cid, const void* buff, size_t len) {
    if (!buff || len == 0) return false;
    return _impl->Send(cid, buff, len);
}

bool Server::SendWithHeader(ConnectionId cid,
        const void* hdr, size_t hdr_len, const void* data, size_t data_len) {

    if (!hdr || hdr_len == 0) {
        return Send(cid, data, data_len);
    }
    return _impl->SendWithHeader(cid, hdr, hdr_len, data, data_len);
}

bool Server::CloseConnection(ConnectionId cid) {
    return _impl->CloseConnection(cid);
}

// user data
bool Server::SetUserData(ConnectionId id, void* userdata) {
    return _impl->SetUserData(id, userdata);
}

bool Server::GetUserData(ConnectionId id, void** userdata) {
    return _impl->GetUserData(id, userdata);
}

bool Server::SetExtendInfo(ConnectionId id, uint64_t info) {
    return _impl->SetExtendInfo(id, info);
}

bool Server::GetExtendInfo(ConnectionId id, uint64_t* info) {
    uint64_t data = 0;
    bool r = _impl->GetExtendInfo(id, data);
    if (info) *info = data;
    return r;
}
int  Server::GetPeerString(ConnectionId cid, char* output, int len) {
    return _impl->GetPeerString(cid, output, len);
}
} // namespace raptor

raptor::ITcpServer* RaptorCreateServer(raptor::IServerReceiver* s) {
    if (!s) return nullptr;
    return new raptor::Server(s);
}

void RaptorReleaseServer(raptor::ITcpServer* server) {
    if (server) delete server;
}
