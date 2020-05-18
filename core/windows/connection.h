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
#include <winsock2.h>
#include "core/resolve_address.h"
#include "core/slice/slice_buffer.h"
#include "util/sync.h"
#include "core/cid.h"

namespace raptor {
namespace internal {
class ITransferService {
public:
    virtual ~ITransferService() {}
    virtual void OnMessage(ConnectionId cid, const Slice& s) = 0;
    virtual void OnClose(ConnectionId cid) = 0;
};

enum class EventType {
    kSendEvent,
    kRecvEvent
};

typedef struct {
    OVERLAPPED overlapped;
    EventType event;
    SOCKET 	fd;
} OverLappedEx;
} // namespace internal

class TcpServer;
class Connection final {
    friend TcpServer;
public:
    Connection(internal::ITransferService* service, ConnectionId cid);
    ~Connection();

    // Before attach, fd must be associated with iocp
    void Init(SOCKET fd, const raptor_resolved_address* addr);
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
    internal::ITransferService * _service;  // not own it
    ConnectionId _cid;
    SOCKET _fd;

    internal::OverLappedEx _send_overlapped;
    internal::OverLappedEx _recv_overlapped;

    raptor_resolved_address _addr;

    SliceBuffer _recv_buffer;
    SliceBuffer _send_buffer;

    Mutex _rcv_mtx;
    Mutex _snd_mtx;

    WSABUF _wsa_snd_buf;
    WSABUF _wsa_rcv_buf;
};
} // namespace raptor
