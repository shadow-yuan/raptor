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
#include "plugin/protocol.h"
#include "util/log.h"
#include "util/useful.h"
#include "core/windows/socket_options.h"

namespace raptor {
Connection::Connection(internal::ITransferService* service, ConnectionId cid)
    : _service(service)
    , _cid(cid)
    , _fd(INVALID_SOCKET) {
    memset(&_send_overlapped, 0, sizeof(_send_overlapped));
    memset(&_recv_overlapped, 0, sizeof(_recv_overlapped));

    _send_overlapped.event = internal::EventType::kSendEvent;
    _recv_overlapped.event = internal::EventType::kRecvEvent;
}

Connection::~Connection() {}

// Before attach, fd must be associated with iocp
void Connection::Init(SOCKET fd, const raptor_resolved_address* addr) {
    _fd = fd;
    _addr = *addr;

    _send_overlapped.fd = fd;
    _recv_overlapped.fd = fd;

    AsyncRecv();
}

void Connection::Shutdown(bool notify) {
    if (_fd != INVALID_SOCKET) {

        raptor_set_socket_shutdown(_fd);
        _fd = INVALID_SOCKET;

        _service->OnClose(_cid);

        _send_overlapped.fd = INVALID_SOCKET;
        _recv_overlapped.fd = INVALID_SOCKET;
    }

    _rcv_mtx.Lock();
    _recv_buffer.ClearBuffer();
    _rcv_mtx.Unlock();

    _snd_mtx.Lock();
    _send_buffer.ClearBuffer();
    _snd_mtx.Unlock();
}

void Connection::Send(const void* data, size_t len) {
    AutoLock g(&_snd_mtx);
    Slice hdr = protocol::BuildPackageHeader(len);
    _send_buffer.AddSlice(hdr);
    _send_buffer.AddSlice(Slice(data, len));
    Slice s = _send_buffer.GetTopSlice();
    AsynSend(&s);
}

bool Connection::AsynSend(Slice* s) {

    _wsa_snd_buf.buf = (char*)s->Buffer();
    _wsa_snd_buf.len = s->Length();

    // https://docs.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-wsasend
    int ret = WSASend(_fd, &_wsa_snd_buf, 1, NULL, 0, &_send_overlapped.overlapped, NULL);

    if (ret == SOCKET_ERROR) {
        int error = WSAGetLastError();
        if (error != WSA_IO_PENDING) {
            return false;
        }
    }
    return true;
}

bool Connection::AsyncRecv() {
    DWORD dwFlag = 0;

    _wsa_rcv_buf.buf = NULL;
    _wsa_rcv_buf.len = 0;

    // https://docs.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-wsarecv
    int ret = WSARecv(_fd, &_wsa_rcv_buf, 1, NULL, &dwFlag, &_recv_overlapped.overlapped, NULL);

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
void Connection::OnSendEvent(size_t size) {
    if (size == 0) return;
    AutoLock g(&_snd_mtx);
    _send_buffer.MoveHeader(size);
    if (_send_buffer.GetBufferLength() > 0) {
        Slice s = _send_buffer.GetTopSlice();
        AsynSend(&s);
    }
}

void Connection::OnRecvEvent(size_t size) {
    if (size == 0) return;
    if (size <= 8192) {
        int r = SyncRecv(size);
        if (r < 0) {
            Shutdown(true);
            return;
        }
        ParsingProtocol();
        return;
    }
    size_t bytes = size;
    do {
        size_t need = RAPTOR_MIN(bytes, 8192);
        size_t real_bytes = 0;
        int n = SyncRecv(need, &real_bytes);
        if (n < 0) {
            Shutdown(true);
            return;
        }
        bytes -= real_bytes;
        ParsingProtocol();

    } while (bytes > 0);
}

int Connection::SyncRecv(size_t size, size_t* real_bytes) {
    int remain_bytes = (int)size;
    int unused_space = 0;
    int recv_bytes = 0;

    do {
        char buffer[8192];
        unused_space = sizeof(buffer);
        int need_read = RAPTOR_MIN(unused_space, remain_bytes);
        recv_bytes = ::recv(_fd, buffer, need_read, 0);
        if (recv_bytes == 0) {
            return -1;
        }

        if (recv_bytes < 0) {
            int error = WSAGetLastError();
            if (error == WSAEWOULDBLOCK) {
                return 0;
            }
            log_error("connection recv error: %d", error);
            return -1;
        }
        _recv_buffer.AddSlice(Slice(buffer, (size_t)recv_bytes));
        remain_bytes -= recv_bytes;

    } while (remain_bytes > 0);
    return 1;
}

void Connection::ParsingProtocol() {
    size_t cache_size = _recv_buffer.GetBufferLength();
    while (cache_size > 0) {
        Slice obj = _recv_buffer.GetHeader(protocol::HeaderSize);
        if (obj.Empty()) {
            return;
        }
        size_t pack_len = protocol::CheckPackageLength(obj.begin(), obj.size());
        if (cache_size < pack_len) {
            return;
        }

        Slice package = _recv_buffer.GetHeader(pack_len);
        _service->OnMessage(_cid, package);
        _recv_buffer.MoveHeader(pack_len);
        cache_size = _recv_buffer.GetBufferLength();
    }
}
} // namespace raptor
