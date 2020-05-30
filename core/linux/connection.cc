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

#include "core/linux/connection.h"
#include <errno.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include "core/linux/epoll_thread.h"
#include "core/linux/socket_setting.h"
#include "raptor/protocol.h"
#include "util/log.h"
#include "util/sync.h"
#include "util/time.h"

namespace raptor {
Connection::Connection(internal::INotificationTransfer* service)
    : _service(service)
    , _proto(nullptr)
    , _fd(-1)
    , _cid(core::InvalidConnectionId)
    , _rcv_thd(nullptr)
    , _snd_thd(nullptr) {

    _user_data = 0;
    _extend_ptr = nullptr;
}

Connection::~Connection() {}

void Connection::Init(
                    ConnectionId cid,
                    int fd,
                    const raptor_resolved_address* addr,
                    SendRecvThread* r, SendRecvThread* s) {
    _cid = cid;
    _rcv_thd = r;
    _snd_thd = s;

    _rcv_thd->Add(fd, (void*)_cid, EPOLLIN | EPOLLET);
    _snd_thd->Add(fd, (void*)_cid, EPOLLOUT | EPOLLET);

    _addr = *addr;

    _service->OnConnectionArrived(_cid, &_addr);
}

bool Connection::Send(const void* ptr, size_t len) {
    if (!IsOnline()) return false;
    AutoMutex g(&_snd_mutex);
    Slice hdr = _proto->BuildPackageHeader(len);
    _snd_buffer.AddSlice(hdr);
    _snd_buffer.AddSlice(Slice(ptr, len));
    _snd_thd->Modify(_fd, (void*)_cid, EPOLLOUT | EPOLLET);
    return true;
}

void Connection::Shutdown(bool notify) {
    if (_fd < 0) {
        return;
    }

    _rcv_thd->Delete(_fd, EPOLLIN | EPOLLET);
    _snd_thd->Delete(_fd, EPOLLOUT | EPOLLET);

    if (notify) {
        _service->OnConnectionClosed(_cid);
    }

    raptor_set_socket_shutdown(_fd);
    _fd = -1;

    memset(&_addr, 0, sizeof(_addr));

    ReleaseBuffer();

    _user_data = 0;
    _extend_ptr = nullptr;
}

bool Connection::IsOnline() {
    return (_fd > 0);
}

const raptor_resolved_address* Connection::GetAddress() {
    return &_addr;
}

void Connection::ReleaseBuffer() {
    {
        AutoMutex g(&_snd_mutex);
        _snd_buffer.ClearBuffer();
    }
    {
        AutoMutex g(&_rcv_mutex);
        _rcv_buffer.ClearBuffer();
    }
}

bool Connection::DoRecvEvent() {
    int result = OnRecv();
    if (result == 0) {
        _rcv_thd->Modify(_fd, (void*)_cid, EPOLLIN | EPOLLET);
        return true;
    }
    Shutdown(true);
    return false;
}

bool Connection::DoSendEvent() {
    int result = OnSend();
    if (result != 0) {
        Shutdown(true);
        return false;
    }
    return true;
}

int Connection::OnRecv() {
    AutoMutex g(&_rcv_mutex);

    int recv_bytes = 0;
    int unused_space = 0;
    do {
        char buffer[8192];

        unused_space = sizeof(buffer);
        recv_bytes = ::recv(_fd, buffer, unused_space, 0);

        if (recv_bytes == 0) {
            return -1;
        }

        if (recv_bytes < 0) {
            if (errno == EINTR || errno == EWOULDBLOCK || errno == EAGAIN) {
                return 0;
            }
            return -1;
        }

        // Add to recv buffer
        _rcv_buffer.AddSlice(Slice(buffer, recv_bytes));

        size_t cache_size = _rcv_buffer.GetBufferLength();
        while (cache_size > 0) {

            Slice obj = _rcv_buffer.GetHeader(_proto->GetMaxHeaderSize());
            if (obj.Empty()) {
                // need more data
                break;
            }

            int pack_len = _proto->CheckPackageLength(&obj);
            if (pack_len <= 0) {
                log_error("connection: internal protocol error(pack_len = %d)", pack_len);
                return -1;
            }

            if (cache_size < (size_t)pack_len) {
                // need more data
                break;
            }

            Slice package = _rcv_buffer.GetHeader((size_t)pack_len);
            _service->OnDataReceived(_cid, &package);
            _rcv_buffer.MoveHeader((size_t)pack_len);
            cache_size = _rcv_buffer.GetBufferLength();
        }
    } while (recv_bytes == unused_space);
    return 0;
}

int Connection::OnSend() {
    AutoMutex g(&_snd_mutex);
    if (_snd_buffer.Empty()) {
        return 0;
    }

    size_t count = 0;
    do {

        Slice slice = _snd_buffer.GetTopSlice();
        int slen = ::send(_fd, slice.begin(), slice.size(), 0);

        if (slen == 0) {
            return -1;
        }

        if (slen < 0) {
            if (errno == EINTR || errno == EWOULDBLOCK || errno == EAGAIN) {
                return 0;
            }
            return -1;
        }

        _snd_buffer.MoveHeader((size_t)slen);
        count = _snd_buffer.Count();

    } while (count > 0);
    return 0;
}

void Connection::SetUserData(void* ptr) {
    _extend_ptr = ptr;
}

void Connection::GetUserData(void** ptr) const {
    if (ptr) {
        *ptr = _extend_ptr;
    }
}

void Connection::SetExtendInfo(uint64_t data) {
    _user_data = data;
}

void Connection::GetExtendInfo(uint64_t& data) const {
    data = _user_data;
}
} // namespace raptor
