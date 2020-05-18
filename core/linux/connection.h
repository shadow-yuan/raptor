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

#ifndef __RAPTOR_CORE_LINUX_CONNECTION__
#define __RAPTOR_CORE_LINUX_CONNECTION__

#include "core/slice/slice_buffer.h"
#include "core/resolve_address.h"
#include "core/service.h"
#include "util/sync.h"

namespace raptor {
class SendRecvThread;

class Connection {
    friend class TcpServer;
public:
    explicit Connection(IMessageTransfer* service);
    ~Connection();

    void Init(
            ConnectionId cid,
            const raptor_resolved_address* addr,
            SendRecvThread* rcv, SendRecvThread* snd);

    bool Send(Slice header, const void* ptr, size_t len);
    void Shutdown(bool notify = false);
    bool IsOnline();
    const raptor_resolved_address* GetAddress();
    ConnectionId Id() const { return _cid; }
    void SetUserData(void* ptr);
    void* GetUserData() const;

private:

    int OnRecv();
    int OnSend();

    void DoRecvEvent();
    void DoSendEvent();
    void ReleaseBuffer();

    IMessageTransfer* _service;

    SendRecvThread* _rcv_thd;
    SendRecvThread* _snd_thd;

    SliceBuffer _rcv_buffer;
    SliceBuffer _snd_buffer;

    Mutex _rcv_mutex;
    Mutex _snd_mutex;

    raptor_resolved_address _addr;

    ConnectionId _cid;

    raptor_socket_t _rst;

    void* _user_data;

};

} // namespace raptor

#endif  // __RAPTOR_CORE_LINUX_CONNECTION__
