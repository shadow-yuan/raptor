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

#ifndef __RAPTOR_SURFACE_ADAPTER__
#define __RAPTOR_SURFACE_ADAPTER__

#include <memory>

#include "raptor/protocol.h"
#include "raptor/service.h"

namespace raptor {
class TcpServer;
class TcpClient;
} // namespace raptor

class RaptorServerAdapter final : public raptor::IServerReceiver
                                , public raptor::ITcpServer {
public:
    RaptorServerAdapter();
    ~RaptorServerAdapter();

    // ITcpServer impl
    bool Init(const RaptorOptions* options) override;
    void SetProtocol(raptor::IProtocol* proto) override;
    bool AddListening(const char* addr) override;
    bool Start() override;
    void Shutdown() override;
    bool Send(ConnectionId cid, const void* buff, size_t len) override;
    bool SendWithHeader(ConnectionId cid, const void* hdr, size_t hdr_len, const void* data, size_t data_len) override;
    bool CloseConnection(ConnectionId cid) override;
    bool SetUserData(ConnectionId id, void* userdata) override;
    bool GetUserData(ConnectionId id, void** userdata) override;
    bool SetExtendInfo(ConnectionId id, uint64_t info) override;
    bool GetExtendInfo(ConnectionId id, uint64_t* info) override;
    int  GetPeerString(ConnectionId cid, char* output, int len) override;

    // IServerReceiver impl
	void OnConnected(ConnectionId id, const char* peer) override;
    void OnMessageReceived(ConnectionId id, const void* buff, size_t len) override;
    void OnClosed(ConnectionId id) override;

    // callbacks (c.h)
    void SetCallbacks(
                    raptor_server_callback_connection_arrived on_arrived,
                    raptor_server_callback_message_received on_message_received,
                    raptor_server_callback_connection_closed on_closed
                    );

private:
    std::shared_ptr<raptor::TcpServer> _impl;
    raptor_server_callback_connection_arrived _on_arrived_cb;
    raptor_server_callback_message_received   _on_message_received_cb;
    raptor_server_callback_connection_closed  _on_closed_cb;
};

class RaptorClientAdapter final : public raptor::ITcpClient
                                , public raptor::IClientReceiver {
public:
    RaptorClientAdapter();
    ~RaptorClientAdapter();

    // ITcpClient impl
    bool Init() override;
    void SetProtocol(raptor::IProtocol* proto) override;
    bool Connect(const char* addr, size_t timeout_ms) override;
    bool Send(const void* buff, size_t len) override;
    void Shutdown() override;

    // IClientReceiver impl
    void OnConnectResult(bool success) override;
    void OnMessageReceived(const void* buff, size_t len) override;
    void OnClosed() override;

    void SetCallbacks(
                    raptor_client_callback_connect_result on_connect_result,
                    raptor_client_callback_message_received on_message_received,
                    raptor_client_callback_connection_closed on_closed
                    );

private:
    std::shared_ptr<raptor::TcpClient> _impl;
    raptor_client_callback_connect_result    _on_connect_result_cb;
    raptor_client_callback_message_received  _on_message_received_cb;
    raptor_client_callback_connection_closed _on_closed_cb;
};

class RaptorProtocolAdapter final : public raptor::IProtocol {
public:
    RaptorProtocolAdapter();
    ~RaptorProtocolAdapter() {}

    // Get the max header size of current protocol
    size_t GetMaxHeaderSize() override;

    // return -1: error;  0: need more data; > 0 : pack_len
    int CheckPackageLength(const void* data, size_t len) override;

    void SetCallbacks(
        raptor_protocol_callback_get_max_header_size get_max_header_size,
        raptor_protocol_callback_check_package_length check_package_length
    );

private:
    raptor_protocol_callback_get_max_header_size _get_max_header_size;
    raptor_protocol_callback_check_package_length _check_package_length;
};

#endif  // __RAPTOR_SURFACE_ADAPTER__
