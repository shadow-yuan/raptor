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
#include <winsock2.h>
#include <string>
#include "core/resolve_address.h"
#include "core/socket_addr.h"
#include "core/status.h"
#include "core/thread.h"
#include "core/slice/slice_buffer.h"
#include "core/sync.h"
#include "raptor/notification.h"

namespace raptor{
class Slice;
class TcpClient final {
public:
    explicit TcpClient(IRaptorClientEvent* service);
    ~TcpClient();

    raptor_error Init();
    raptor_error Connect(const std::string& addr, size_t timeout_ms);
    raptor_error Send(const void* buff, size_t len);
    raptor_error Shutdown();

private:
    void WorkThread();
    raptor_error InternalConnect(const raptor_resolved_address* addr);
    raptor_error GetConnectExIfNecessary(SOCKET s);

    void OnConnectedEvent(int err);
    void OnCloseEvent(int err);
    void OnReadEvent(int err);
    void OnSendEvent(int err);

    bool AsyncSend(Slice* s);
    void AsyncRecv();

    IRaptorClientEvent *_service;
    bool _progressing;
    bool _shutdown;
    LPFN_CONNECTEX _connectex;

    SOCKET _fd;
    WSAEVENT _event;

    Thread _thd;

    OVERLAPPED _c_overlapped;
    OVERLAPPED _s_overlapped;
    OVERLAPPED _r_overlapped;

    Mutex _s_mtx;
    Mutex _r_mtx;

    SliceBuffer _snd_buffer;
    SliceBuffer _rcv_buffer;

    WSABUF _wsa_snd_buf;
    WSABUF _wsa_rcv_buf;
};
} // namespace raptor
