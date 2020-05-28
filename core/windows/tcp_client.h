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
#include <string>
#include "core/resolve_address.h"
#include "core/sockaddr.h"
#include "util/status.h"
#include "util/thread.h"
#include "core/slice/slice_buffer.h"
#include "raptor/service.h"
#include "raptor/protocol.h"
#include "raptor/slice.h"
#include "util/sync.h"

namespace raptor{
class TcpClient final {
public:
    TcpClient(ITcpClientService* service, Protocol* proto);
    ~TcpClient();

    raptor_error Init();
    raptor_error Connect(const std::string& addr, size_t timeout_ms);
    bool Send(const void* buff, size_t len);
    void Shutdown();
    bool IsOnline() const;
private:
    void WorkThread(void* ptr);
    raptor_error InternalConnect(const raptor_resolved_address* addr);
    raptor_error GetConnectExIfNecessary(SOCKET s);

    // network event
    void OnConnectEvent(int err);
    void OnCloseEvent(int err);
    void OnReadEvent(int err);
    void OnSendEvent(int err);

    bool AsyncSend();
    bool AsyncRecv();

    bool DoSend();
    bool DoRecv();
    void ParsingProtocol();

private:
    enum { DEFAULT_TEMP_SLICE_COUNT = 2};
    ITcpClientService *_service;
    Protocol* _proto;
    bool _send_pending;
    bool _shutdown;
    LPFN_CONNECTEX _connectex;

    SOCKET _fd;
    WSAEVENT _event;

    Thread _thd;

    OVERLAPPED _conncet_overlapped;
    OVERLAPPED _send_overlapped;
    OVERLAPPED _recv_overlapped;

    Mutex _s_mtx;
    Mutex _r_mtx;

    SliceBuffer _snd_buffer;
    SliceBuffer _rcv_buffer;

    Slice _tmp_buffer[DEFAULT_TEMP_SLICE_COUNT];
};
} // namespace raptor
