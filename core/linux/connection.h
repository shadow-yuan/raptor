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

#include "core/resolve_address.h"
#include "core/service.h"
#include "core/slice/slice_buffer.h"
#include "util/sync.h"

namespace raptor {

class IProtocol;
class SendRecvThread;

class Connection {
    friend class TcpServer;
public:
    explicit Connection(internal::INotificationTransfer* service);
    ~Connection();

    void Init(
            ConnectionId cid,
            int fd,
            const raptor_resolved_address* addr,
            SendRecvThread* rcv, SendRecvThread* snd);

    void SetProtocol(IProtocol* p);
    bool SendWithHeader(
        const void* hdr, size_t hdr_len, const void* data, size_t data_len);
    void Shutdown(bool notify = false);
    bool IsOnline();
    const raptor_resolved_address* GetAddress();
    ConnectionId Id() const { return _cid; }
    void SetUserData(void* ptr);
    void GetUserData(void** ptr) const;
    void SetExtendInfo(uint64_t data);
    void GetExtendInfo(uint64_t& data) const;
    int GetPeerString(char* buf, int buf_size);

private:

    int OnRecv();
    int OnSend();

    bool DoRecvEvent();
    bool DoSendEvent();
    void ReleaseBuffer();

    // if success return the number of parsed packets
    // otherwise return -1 (protocol error)
    int  ParsingProtocol();

    // return true if reach recv buffer tail.
    bool ReadSliceFromRecvBuffer(size_t read_size, Slice& s);

    internal::INotificationTransfer* _service;
    IProtocol* _proto;
    int _fd;
    ConnectionId _cid;

    SendRecvThread* _rcv_thd;
    SendRecvThread* _snd_thd;

    SliceBuffer _rcv_buffer;
    SliceBuffer _snd_buffer;

    Mutex _rcv_mutex;
    Mutex _snd_mutex;

    raptor_resolved_address _addr;
    Slice _addr_str;

    uint64_t _user_data;
    void* _extend_ptr;
};

} // namespace raptor

#endif  // __RAPTOR_CORE_LINUX_CONNECTION__
