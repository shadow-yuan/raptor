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

#include "core/windows/connection.h"
#include <string.h>
#include "core/socket_util.h"
#include "core/windows/socket_setting.h"
#include "raptor/protocol.h"
#include "util/log.h"
#include "util/useful.h"

namespace raptor {
Connection::Connection(internal::INotificationTransfer* service)
    : _service(service)
    , _proto(nullptr)
    , _send_pending(false)
    , _cid(core::InvalidConnectionId)
    , _fd(INVALID_SOCKET) {

    memset(&_send_overlapped, 0, sizeof(_send_overlapped));
    memset(&_recv_overlapped, 0, sizeof(_recv_overlapped));
    _send_overlapped.event = IocpEventType::kSendEvent;
    _recv_overlapped.event = IocpEventType::kRecvEvent;

    memset(&_addr, 0, sizeof(_addr));

    _user_data = 0;
    _extend_ptr = 0;
}

Connection::~Connection() {}

void Connection::Init(ConnectionId cid, SOCKET sock, const raptor_resolved_address* addr) {
    _cid = cid;
    _fd = sock;
    _addr = *addr;
    _send_pending = false;

    _user_data = 0;
    _extend_ptr = 0;

    for(size_t i = 0; i < DEFAULT_TEMP_SLICE_COUNT; i++) {
        _tmp_buffer[i] = MakeSliceByDefaultSize();
    }
    _service->OnConnectionArrived(cid, &_addr);
}

void Connection::SetProtocol(Protocol* p) {
    _proto = p;
}

void Connection::Shutdown(bool notify) {
    if (_fd == INVALID_SOCKET) {
        return;
    }

    raptor_set_socket_shutdown(_fd);
    _fd = INVALID_SOCKET;
    _send_pending = false;

    if (notify) {
        _service->OnConnectionClosed(_cid);
    }

    _rcv_mtx.Lock();
    _recv_buffer.ClearBuffer();
    _rcv_mtx.Unlock();

    _snd_mtx.Lock();
    _send_buffer.ClearBuffer();
    _snd_mtx.Unlock();

    for (size_t i = 0; i < DEFAULT_TEMP_SLICE_COUNT; i++) {
        _tmp_buffer[i] = Slice();
    }
    _user_data = 0;
    _extend_ptr = 0;
}

bool Connection::Send(const void* data, size_t len) {
    AutoMutex g(&_snd_mtx);
    Slice hdr = _proto->BuildPackageHeader(len);
    _send_buffer.AddSlice(hdr);
    _send_buffer.AddSlice(Slice(data, len));
    return AsyncSend();
}

constexpr size_t MAX_PACKAGE_SIZE = 0xffff;
constexpr size_t MAX_WSABUF_COUNT = 16;

bool Connection::AsyncSend() {
    if (_send_buffer.Empty() || _send_pending) {
        return true;
    }

    WSABUF wsa_snd_buf[MAX_WSABUF_COUNT];
    size_t prepare_send_length = 0;
    DWORD wsa_buf_count = 0;

    _send_pending = true;

    size_t count = _send_buffer.Count();
    for (size_t i = 0; i < count && i < MAX_WSABUF_COUNT; i++) {
        Slice s = _send_buffer.GetSlice(i);
        if (prepare_send_length + s.Length() > MAX_PACKAGE_SIZE) {
            break;
        }
        wsa_snd_buf[i].buf = reinterpret_cast<char*>(s.Buffer());
        wsa_snd_buf[i].len = static_cast<ULONG>(s.Length());
        prepare_send_length += s.Length();
        wsa_buf_count++;
    }

    // https://docs.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-wsasend
    int ret = WSASend(_fd, wsa_snd_buf, wsa_buf_count, NULL, 0, &_send_overlapped.overlapped, NULL);

    if (ret == SOCKET_ERROR) {
        int error = WSAGetLastError();
        if (error != WSA_IO_PENDING) {
            _send_pending = false;
            return false;
        }
    }
    return true;
}

bool Connection::AsyncRecv() {
    DWORD dwFlag = 0;
    WSABUF wsa_rcv_buf[DEFAULT_TEMP_SLICE_COUNT];

    for (size_t i = 0; i < DEFAULT_TEMP_SLICE_COUNT; i++) {
        wsa_rcv_buf[i].buf = reinterpret_cast<char*>(_tmp_buffer[i].Buffer());
        wsa_rcv_buf[i].len = static_cast<ULONG>(_tmp_buffer[i].Length());
    }

    // https://docs.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-wsarecv
    int ret = WSARecv(_fd, wsa_rcv_buf, DEFAULT_TEMP_SLICE_COUNT, NULL, &dwFlag, &_recv_overlapped.overlapped, NULL);

    if (ret == SOCKET_ERROR) {
        int error = WSAGetLastError();
        if (error != WSA_IO_PENDING) {
            return false;
        }
    }
    return true;
}

bool Connection::IsOnline() {
    return (_fd != INVALID_SOCKET);
}

// IOCP Event
bool Connection::OnSendEvent(size_t size) {
    RAPTOR_ASSERT(size != 0);
    AutoMutex g(&_snd_mtx);
    _send_pending = false;
    _send_buffer.MoveHeader(size);
    if (_send_buffer.Empty()) {
        return true;
    }
    return AsyncSend();
}

bool Connection::OnRecvEvent(size_t size) {
    RAPTOR_ASSERT(size != 0);
    AutoMutex g(&_rcv_mtx);
    size_t node_size = _tmp_buffer[0].size();
    size_t count = size / node_size;
    size_t i = 0;

    for (; i < DEFAULT_TEMP_SLICE_COUNT && i < count; i++) {
        _recv_buffer.AddSlice(_tmp_buffer[i]);
        _tmp_buffer[i] = MakeSliceByDefaultSize();
        size -= node_size;
    }

    if (size > 0) {
        Slice s(_tmp_buffer[i].Buffer(), size);
        _recv_buffer.AddSlice(s);
    }

    if (!ParsingProtocol()) {
        return false;
    }
    return AsyncRecv();
}

bool Connection::ParsingProtocol() {
    size_t cache_size = _recv_buffer.GetBufferLength();
    while (cache_size > 0) {
        Slice obj = _recv_buffer.GetHeader(_proto->GetMaxHeaderSize());
        if (obj.Empty()) {
            break;
        }
        int pack_len = _proto->CheckPackageLength(&obj);
        if (pack_len <= 0) {
            log_error("connection: internal protocol error(pack_len = %d)", pack_len);
            return false;
        }

        if (cache_size < (size_t)pack_len) {
            break;
        }

        Slice package = _recv_buffer.GetHeader(pack_len);
        _service->OnDataReceived(_cid, &package);
        _recv_buffer.MoveHeader(pack_len);
        cache_size = _recv_buffer.GetBufferLength();
    }
    return true;
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
