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

#ifndef __RAPTOR_CORE_LINUX_TCP_CLIENT__
#define __RAPTOR_CORE_LINUX_TCP_CLIENT__

#include "core/resolve_address.h"
#include "core/sockaddr.h"
#include "core/slice/slice.h"
#include "core/slice/slice_buffer.h"
#include "raptor/service.h"
#include "raptor/protocol.h"
#include "util/status.h"
#include "util/sync.h"
#include "util/thread.h"

namespace raptor{
class TcpClient final {
public:
    explicit TcpClient(IClientReceiver* service);
    ~TcpClient();

    raptor_error Init();
    raptor_error Connect(const char* addr, size_t timeout_ms);
    bool Send(const void* buff, size_t len);
    void Shutdown();
    bool IsOnline() const;
    void SetProtocol(IProtocol* proto);

private:
    void WorkThread(void* ptr);

    int DoSend();
    int DoRecv();

    raptor_error AsyncConnect(
        const raptor_resolved_address* addr, int timeout_ms, int* new_fd);

    // if success return the number of parsed packets
    // otherwise return -1 (protocol error)
    int  ParsingProtocol();

    // return true if reach recv buffer tail.
    bool ReadSliceFromRecvBuffer(size_t read_size, Slice& s);

private:
    IClientReceiver *_service;
    IProtocol* _proto;

    bool _shutdown;
    bool _is_connected;

    int _fd;

    Thread _thd;

    Mutex _s_mtx;
    Mutex _r_mtx;

    SliceBuffer _snd_buffer;
    SliceBuffer _rcv_buffer;

};
} // namespace raptor
#endif  // __RAPTOR_CORE_LINUX_TCP_CLIENT__
