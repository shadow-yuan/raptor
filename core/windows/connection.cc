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
#include "util/alloc.h"
#include "util/log.h"
#include "util/useful.h"

namespace raptor {
Connection::Connection(internal::INotificationTransfer* service)
    : _service(service)
    , _proto(nullptr)
    , _send_pending(false)
    , _cid(core::InvalidConnectionId)
    , _fd(INVALID_SOCKET) {

    memset(&_send_overlapped.overlapped, 0, sizeof(_send_overlapped.overlapped));
    memset(&_recv_overlapped.overlapped, 0, sizeof(_recv_overlapped.overlapped));
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

    char* output = nullptr;
    int bytes = raptor_sockaddr_to_string(&output, &_addr, 1);
    if (output && bytes > 0) {
        _addr_str = Slice(output, static_cast<size_t>(bytes+1));
    }
    if (output) {
        Free(output);
    }
    _service->OnConnectionArrived(cid, &_addr_str);
}

void Connection::SetProtocol(IProtocol* p) {
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
    _rcv_buffer.ClearBuffer();
    _rcv_mtx.Unlock();

    _snd_mtx.Lock();
    _snd_buffer.ClearBuffer();
    _snd_mtx.Unlock();

    for (size_t i = 0; i < DEFAULT_TEMP_SLICE_COUNT; i++) {
        _tmp_buffer[i] = Slice();
    }
    _user_data = 0;
    _extend_ptr = 0;
}

bool Connection::SendWithHeader(
        const void* hdr, size_t hdr_len, const void* data, size_t data_len) {
    if (!IsOnline()) return false;
    AutoMutex g(&_snd_mtx);
    if (hdr != nullptr && hdr_len > 0) {
        _snd_buffer.AddSlice(Slice(hdr, hdr_len));
    }
    if (data != nullptr && data_len > 0) {
        _snd_buffer.AddSlice(Slice(data, data_len));
    }
    return AsyncSend();
}

constexpr size_t MAX_PACKAGE_SIZE = 0xffff;
constexpr size_t MAX_WSABUF_COUNT = 16;

bool Connection::AsyncSend() {
    if (_snd_buffer.Empty() || _send_pending) {
        return true;
    }

    WSABUF wsa_snd_buf[MAX_WSABUF_COUNT];
    size_t prepare_send_length = 0;
    DWORD wsa_buf_count = 0;

    _send_pending = true;

    size_t count = _snd_buffer.Count();
    for (size_t i = 0; i < count && i < MAX_WSABUF_COUNT; i++) {
        Slice s = _snd_buffer.GetSlice(i);
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
    _snd_buffer.MoveHeader(size);
    if (_snd_buffer.Empty()) {
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
        _rcv_buffer.AddSlice(_tmp_buffer[i]);
        _tmp_buffer[i] = MakeSliceByDefaultSize();
        size -= node_size;
    }

    if (size > 0) {
        Slice s(_tmp_buffer[i].Buffer(), size);
        _rcv_buffer.AddSlice(s);
    }

    if (ParsingProtocol() == -1) {
        return false;
    }
    return AsyncRecv();
}

bool Connection::ReadSliceFromRecvBuffer(size_t read_size, Slice& s) {
    size_t cache_size = _rcv_buffer.GetBufferLength();
    if (read_size >= cache_size) {
        s = _rcv_buffer.Merge();
        return true;
    }
    s = _rcv_buffer.GetHeader(read_size);
    return false;
}

int Connection::ParsingProtocol() {
    size_t cache_size = _rcv_buffer.GetBufferLength();
    size_t header_size = _proto->GetMaxHeaderSize();
    int package_counter = 0;

    while (cache_size > 0) {
        size_t read_size = header_size;
        int pack_len = 0;
        Slice package;
        do {
            bool reach_tail = ReadSliceFromRecvBuffer(read_size, package);
            pack_len = _proto->CheckPackageLength(package.begin(), package.size());
            if (pack_len < 0) {
                log_error("tcp client: internal protocol error(pack_len = %d)", pack_len);
                return -1;
            }

            // equal 0 means we need more data
            if (pack_len == 0) {
                if (reach_tail) {
                    goto done;
                }
                read_size *= 2;
                continue;
            }

            // We got the length of a whole packet
            if (cache_size >= (size_t)pack_len) {
                break;
            }
            goto done;
        } while (false);

        if (package.size() < static_cast<size_t>(pack_len)) {
            package = _rcv_buffer.GetHeader(pack_len);
        } else {
            size_t n = package.size() - pack_len;
            package.CutTail(n);
        }
        _service->OnDataReceived(_cid, &package);
        _rcv_buffer.MoveHeader(pack_len);

        cache_size = _rcv_buffer.GetBufferLength();
        package_counter++;
    }
done:
    return package_counter;
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

int Connection::GetPeerString(char* buf, int len) {
    if (!IsOnline()) {
        return -1;
    }

    int bytes = static_cast<int>(_addr_str.size());
    if (len > 0 && bytes > 0) {
        if (bytes > len) {
            bytes = len;
        }
        memcpy(buf, _addr_str.begin(), bytes);
        return bytes;
    }
    return 0;
}

} // namespace raptor
