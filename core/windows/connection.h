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
#include <utility>
#include <vector>

#include "core/cid.h"
#include "core/resolve_address.h"
#include "core/service.h"
#include "core/slice/slice_buffer.h"
#include "core/windows/iocp.h"
#include "util/sync.h"
#include "raptor/slice.h"

namespace raptor {
class Protocol;
constexpr int DEFAULT_TEMP_SLICE_COUNT = 2;
class Connection final {
    friend class TcpServer;
public:
    explicit Connection(internal::INotificationTransfer* service);
    ~Connection();

    // Before Init, sock must be associated with iocp
    void Init(ConnectionId cid, SOCKET sock, const raptor_resolved_address* addr);
    void SetProtocol(Protocol* p);
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
    bool AsynSend();
    bool AsyncRecv();

private:
    using SliceEx = std::pair<Slice, int>;

    internal::INotificationTransfer * _service;
    Protocol* _proto;

    ConnectionId _cid;
    bool _send_pending;

    SOCKET _fd;

    OverLappedEx _send_overlapped;
    OverLappedEx _recv_overlapped;

    raptor_resolved_address _addr;

    SliceBuffer _recv_buffer;
    SliceBuffer _send_buffer;

    SliceEx _tmp_buffer[DEFAULT_TEMP_SLICE_COUNT];

    Protocol* _proto;

    Mutex _rcv_mtx;
    Mutex _snd_mtx;
};
} // namespace raptor
