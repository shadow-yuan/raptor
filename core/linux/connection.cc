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
#include "core/socket_util.h"
#include "util/sync.h"
#include "plugin/protocol.h"
#include "util/time.h"
#include "util/log.h"

namespace raptor {
Connection::Connection(IMessageTransfer* service)
    : _service(service)
    , _rcv_thd(nullptr)
    , _snd_thd(nullptr) {

    _cid = core::InvalidConnectionId;

    _user_data = nullptr;
}

Connection::~Connection() {}

void Connection::Init(
                    ConnectionId cid,
                    const raptor_resolved_address* addr,
                    SendRecvThread* r, SendRecvThread* s) {
    _cid = cid;
    _rcv_thd = r;
    _snd_thd = s;

    _rcv->Add(_cid.s.fd, (void*)_cid.value, EPOLLIN | EPOLLET);
    _snd->Add(_cid.s.fd, (void*)_cid.value, EPOLLOUT | EPOLLET);

    _addr = *addr;
}

bool Connection::Send(Slice header, const void* ptr, size_t len) {
    AutoMutex g(&_snd_mutex);
    _snd_buffer.AddSlice(header);
    _snd_buffer.AddSlice(Slice(ptr, len));
    _snd->Modify(_cid.s.fd, (void*)_cid.value, EPOLLOUT | EPOLLET);
    return true;
}

void Connection::Shutdown(bool notify) {
    if (_rst.fd <= 0) {
        return;
    }

    if (_rcv_thd) {
        _rcv_thd->Delete(_cid.s.fd, EPOLLIN | EPOLLET);
    }
    if (_snd_thd) {
        _snd_thd->Delete(_cid.s.fd, EPOLLOUT | EPOLLET);
    }

    if (notify && _service) {
        _service->OnClose(_cid);
    }

    raptor_set_socket_shutdown(_cid.s.fd);

    _rst.fd = 0;

    memset(&_addr, 0, sizeof(_addr));

    ReleaseBuffer();
}

bool Connection::IsOnline() {
    return _cid.s.fd > 0;
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

void Connection::DoRecvEvent() {
    int result = OnRecv();
    if (result == 0) {
        _rcv->Modify(_cid.s.fd, (void*)_cid.value, EPOLLIN | EPOLLET);
    } else {
        Shutdown(true);
    }
}

void Connection::DoSendEvent() {
    int result = OnSend();
    if (result != 0) {
        // close connection
    }
}

int Connection::OnRecv() {
    AutoMutex g(&_rcv_mutex);

    int recv_bytes = 0;
    int unused_space = 0;
    do {
        char buffer[8192];

        unused_space = sizeof(buffer);
        recv_bytes = ::recv(_cid.s.fd, buffer, unused_space, 0);

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

            Slice obj = _rcv_buffer.GetHeader(Protocol::HeaderSize);
            if (obj.Empty()) {
                // need more data
                break;
            }

            size_t pack_len = (size_t)Protocol::CheckPackage(obj.begin(), obj.size());
            if (cache_size < pack_len) {
                // need more data
                break;
            } else if (pack_len == 0) {
                log_error("connection: internal protocol error(pack_len = 0)");
                return -1;
            } else if (cache_size == pack_len) {
                obj = _rcv_buffer.Merge();
                _service->OnMessage(_cid, &obj);
                _rcv_buffer.ClearBuffer();
            } else {
                obj = _rcv_buffer.GetHeader(pack_len);
                _service->OnMessage(_cid, &obj);
                _rcv_buffer.MoveHeader(pack_len);
            }
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
        int slen = ::send(_cid.s.fd, slice.begin(), slice.size(), 0);

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
    _user_data = ptr;
}

void* Connection::GetUserData() const {
    return _user_data;
}
} // namespace raptor
