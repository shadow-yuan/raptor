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
#include <stdint.h>

#include "core/windows/iocp.h"
#include "core/resolve_address.h"
#include "core/slice/slice_buffer.h"
#include "util/sync.h"
#include "core/cid.h"
#include "core/service.h"

namespace raptor {
class TcpServer;
class Connection final {
    friend TcpServer;
public:
    explicit Connection(internal::IMessageTransfer* service);
    ~Connection();

    // Before attach, fd must be associated with iocp
    void Init(ConnectionId cid, raptor_socket_t sock, const raptor_resolved_address* addr);
    void Shutdown(bool notify);

    void Send(const void* data, size_t len);
    bool IsOnline();

private:
    void Recv();

    // IOCP Event
    void OnSendEvent(size_t size);
    void OnRecvEvent(size_t size);

    void ParsingProtocol();
    int  SyncRecv(size_t size, size_t* real_bytes = nullptr);
    bool AsynSend(Slice* s);
    bool AsyncRecv();

private:
    internal::IMessageTransfer * _service;  // not own it
    ConnectionId _cid;
    raptor_socket_t _sock;

    OverLappedEx _send_overlapped;
    OverLappedEx _recv_overlapped;

    raptor_resolved_address _addr;

    SliceBuffer _recv_buffer;
    SliceBuffer _send_buffer;

    Mutex _rcv_mtx;
    Mutex _snd_mtx;

    WSABUF _wsa_snd_buf;
    WSABUF _wsa_rcv_buf;
};
} // namespace raptor
