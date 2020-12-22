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

#include "surface/adapter.h"
#ifdef _WIN32
#include "core/windows/tcp_server.h"
#include "core/windows/tcp_client.h"
#else
#include "core/linux/tcp_server.h"
#include "core/linux/tcp_client.h"
#endif

#include "util/log.h"
#include "util/status.h"
#include "util/useful.h"

RaptorServerAdapter::RaptorServerAdapter()
    : _impl(std::make_shared<raptor::TcpServer>(this)) {
}

RaptorServerAdapter::~RaptorServerAdapter() {}

bool RaptorServerAdapter::Init(const RaptorOptions* options) {
    raptor_error e = _impl->Init(options);
    if (e != RAPTOR_ERROR_NONE) {
        log_error("server adapter: init (%s)", e->ToString().c_str());
        return false;
    }
    return true;
}

void RaptorServerAdapter::SetProtocol(raptor::IProtocol* proto) {
    _impl->SetProtocol(proto);
}

bool RaptorServerAdapter::AddListening(const char* addr) {
    raptor_error e = _impl->AddListening(addr);
    if (e != RAPTOR_ERROR_NONE) {
        log_error("server adapter: add listening (%s)", e->ToString().c_str());
        return false;
    }
    return true;
}

bool RaptorServerAdapter::Start() {
    raptor_error e = _impl->Start();
    if (e != RAPTOR_ERROR_NONE) {
        log_error("server adapter: start (%s)", e->ToString().c_str());
        return false;
    }

    return true;
}

void RaptorServerAdapter::Shutdown() {
    _impl->Shutdown();
}

bool RaptorServerAdapter::Send(ConnectionId cid, const void* buff, size_t len) {
    if (!buff || len == 0) return false;
    return _impl->Send(cid, buff, len);
}

bool RaptorServerAdapter::SendWithHeader(
    ConnectionId cid, const void* hdr, size_t hdr_len, const void* data, size_t data_len) {
    if (!hdr || hdr_len == 0) {
        return Send(cid, data, data_len);
    }
    return _impl->SendWithHeader(cid, hdr, hdr_len, data, data_len);
}

bool RaptorServerAdapter::CloseConnection(ConnectionId cid) {
    return _impl->CloseConnection(cid);
}

void RaptorServerAdapter::OnConnected(ConnectionId id, const char* peer) {
    if (_on_arrived_cb) {
        _on_arrived_cb(id, peer);
    }
}

void RaptorServerAdapter::OnMessageReceived(ConnectionId id, const void* buff, size_t len) {
    if (_on_message_received_cb) {
        _on_message_received_cb(id, buff, len);
    }
}

void RaptorServerAdapter::OnClosed(ConnectionId id) {
    if (_on_closed_cb) {
        _on_closed_cb(id);
    }
}

// callbacks
void RaptorServerAdapter::SetCallbacks(
                                    raptor_server_callback_connection_arrived on_arrived,
                                    raptor_server_callback_message_received on_message_received,
                                    raptor_server_callback_connection_closed on_closed
                                    ) {
    _on_arrived_cb = on_arrived;
    _on_message_received_cb = on_message_received;
    _on_closed_cb = on_closed;
}

// user data
bool RaptorServerAdapter::SetUserData(ConnectionId id, void* userdata) {
    return _impl->SetUserData(id, userdata);
}

bool RaptorServerAdapter::GetUserData(ConnectionId id, void** userdata) {
    return _impl->GetUserData(id, userdata);
}

bool RaptorServerAdapter::SetExtendInfo(ConnectionId id, uint64_t info) {
    return _impl->SetExtendInfo(id, info);
}

bool RaptorServerAdapter::GetExtendInfo(ConnectionId id, uint64_t* info) {
    uint64_t data = 0;
    bool r = _impl->GetExtendInfo(id, data);
    if (info) *info = data;
    return r;
}

int RaptorServerAdapter::GetPeerString(ConnectionId cid, char* output, int len) {
    if (!output || len <= 0) return -1;
    return _impl->GetPeerString(cid, output, len);
}
// --------------------------------

RaptorClientAdapter::RaptorClientAdapter()
    : _impl(std::make_shared<raptor::TcpClient>(this)) {
}

RaptorClientAdapter::~RaptorClientAdapter() {}

bool RaptorClientAdapter::Init() {
    raptor_error e = _impl->Init();
    if (e != RAPTOR_ERROR_NONE) {
        log_error("client adapter: init (%s)", e->ToString().c_str());
        return false;
    }
    return true;
}

void RaptorClientAdapter::SetProtocol(raptor::IProtocol* proto) {
    _impl->SetProtocol(proto);
}

bool RaptorClientAdapter::Connect(const char* addr, size_t timeout_ms) {
    raptor_error e = _impl->Connect(addr, timeout_ms);
    if (e != RAPTOR_ERROR_NONE) {
        log_error("server adapter: connect (%s)", e->ToString().c_str());
        return false;
    }
    return true;
}

bool RaptorClientAdapter::Send(const void* buff, size_t len) {
    return _impl->Send(buff, len);
}

void RaptorClientAdapter::Shutdown() {
    _impl->Shutdown();
}

void RaptorClientAdapter::OnConnectResult(bool success) {
    if (_on_connect_result_cb) {
        _on_connect_result_cb(success ? 1 : 0);
    }
}

void RaptorClientAdapter::OnMessageReceived(const void* buff, size_t len) {
    if (_on_message_received_cb) {
        _on_message_received_cb(buff, len);
    }
}

void RaptorClientAdapter::OnClosed() {
    if (_on_closed_cb) {
        _on_closed_cb();
    }
}

void RaptorClientAdapter::SetCallbacks(
                                    raptor_client_callback_connect_result on_connect_result,
                                    raptor_client_callback_message_received on_message_received,
                                    raptor_client_callback_connection_closed on_closed
                                    ) {
    _on_connect_result_cb = on_connect_result;
    _on_message_received_cb = on_message_received;
    _on_closed_cb = on_closed;
}

// ----------------------------------------

RaptorProtocolAdapter::RaptorProtocolAdapter() {
    _get_max_header_size = nullptr;
    _check_package_length = nullptr;
}

// Get the max header size of current protocol
size_t RaptorProtocolAdapter::GetMaxHeaderSize() {
    return _get_max_header_size();
}

// return -1: error;  0: need more data; > 0 : pack_len
int RaptorProtocolAdapter::CheckPackageLength(const void* data, size_t len) {
    return _check_package_length(data, len);
}

void RaptorProtocolAdapter::SetCallbacks(
        raptor_protocol_callback_get_max_header_size cb1,
        raptor_protocol_callback_check_package_length cb3) {
    _get_max_header_size = cb1;
    _check_package_length = cb3;
}
