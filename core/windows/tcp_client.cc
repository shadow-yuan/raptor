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
#include "core/windows/tcp_client.h"
#include <string.h>
#include "core/socket_util.h"
#include "core/windows/socket_setting.h"
#include "util/log.h"

namespace raptor {
TcpClient::TcpClient(IClientReceiver* service)
    : _service(service)
    , _proto(nullptr)
    , _send_pending(false)
    , _shutdown(true)
    , _connectex(nullptr)
    , _fd(INVALID_SOCKET)
    , _event(WSA_INVALID_EVENT) {
    memset(&_conncet_overlapped, 0, sizeof(_conncet_overlapped));
    memset(&_send_overlapped, 0, sizeof(_send_overlapped));
    memset(&_recv_overlapped, 0, sizeof(_recv_overlapped));
}

TcpClient::~TcpClient() {}

raptor_error TcpClient::Init() {
    if (!_shutdown) {
        return RAPTOR_ERROR_FROM_STATIC_STRING("tcp client already running");
    }

    _event = WSACreateEvent();
    if (_event == WSA_INVALID_EVENT) {
        return RAPTOR_WINDOWS_ERROR(WSAGetLastError(), "WSACreateEvent");
    }

    _shutdown = false;
    _send_pending = false;

    for (size_t i = 0; i < DEFAULT_TEMP_SLICE_COUNT; i++) {
        _tmp_buffer[i] = MakeSliceByDefaultSize();
    }

    _thd = Thread("client",
        std::bind(&TcpClient::WorkThread, this, std::placeholders::_1), nullptr);

    _thd.Start();
    return RAPTOR_ERROR_NONE;
}

raptor_error TcpClient::Connect(const char* addr, size_t timeout_ms) {
    (void)timeout_ms; // ignore

    if (_shutdown) {
        return RAPTOR_ERROR_FROM_STATIC_STRING("TcpClient is not initialized");
    }
    if (_send_pending) {
        return RAPTOR_ERROR_FROM_STATIC_STRING("Connection operation in progress");
    }
    raptor_resolved_addresses* addrs;
    auto e = raptor_blocking_resolve_address(addr, nullptr, &addrs);
    if (e != RAPTOR_ERROR_NONE) {
        return e;
    }
    RAPTOR_ASSERT(addrs->naddrs > 0);
    e = InternalConnect(&addrs->addrs[0]);
    raptor_resolved_addresses_destroy(addrs);
    return e;
}

raptor_error TcpClient::GetConnectExIfNecessary(SOCKET s) {
    if (!_connectex) {
        GUID guid = WSAID_CONNECTEX;
        DWORD ioctl_num_bytes;
        int status;

        /* Grab the function pointer for ConnectEx for that specific socket.
            It may change depending on the interface. */
        status =
            WSAIoctl(s, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid, sizeof(guid),
                    &_connectex, sizeof(_connectex), &ioctl_num_bytes, NULL, NULL);

        if (status != 0) {
            return RAPTOR_WINDOWS_ERROR(WSAGetLastError(), "WSAIoctl(SIO_GET_EXTENSION_FUNCTION_POINTER)");
        }
    }
    return RAPTOR_ERROR_NONE;
}

raptor_error TcpClient::InternalConnect(const raptor_resolved_address* addr) {
    raptor_dualstack_mode mode;
    raptor_resolved_address local_address;
    raptor_resolved_address mapped_addr;
    int status;
    BOOL ret;

    raptor_error error = raptor_create_socket(addr, &mapped_addr, &_fd, &mode);

    if (error != RAPTOR_ERROR_NONE) {
        goto failure;
    }
    error = raptor_tcp_prepare_socket(_fd);
    if (error != RAPTOR_ERROR_NONE) {
        goto failure;
    }
    error = GetConnectExIfNecessary(_fd);
    if (error != RAPTOR_ERROR_NONE) {
        goto failure;
    }

    raptor_sockaddr_make_wildcard6(0, &local_address);

    status =
        bind(_fd, (raptor_sockaddr*)&local_address.addr, (int)local_address.len);

    if (status != 0) {
        error = RAPTOR_WINDOWS_ERROR(WSAGetLastError(), "bind");
        goto failure;
    }

    /* Associate event type FD_CONNECT with the fd and event */
    WSAEventSelect(_fd, _event, FD_CONNECT);

    ret = _connectex(_fd,
                    (raptor_sockaddr*)&mapped_addr.addr, (int)mapped_addr.len,
                    NULL, 0, NULL, &_conncet_overlapped);

    /* It wouldn't be unusual to get a success immediately. But we'll still get
        an IOCP notification, so let's ignore it. */
    if (!ret) {
        int last_error = WSAGetLastError();
        if (last_error != ERROR_IO_PENDING) {
            error = RAPTOR_WINDOWS_ERROR(WSAGetLastError(), "ConnectEx");
            goto failure;
        }
    }

    return RAPTOR_ERROR_NONE;
failure:
    if (_fd != INVALID_SOCKET) {
        closesocket(_fd);
        _fd = INVALID_SOCKET;
    }
    return error;
}

bool TcpClient::Send(const void* buff, size_t len) {
    if (!IsOnline()) {
        return false;
    }

    AutoMutex g(&_s_mtx);
    _snd_buffer.AddSlice(Slice(buff, len));
    if (!_send_pending) {
        return AsyncSend();
    }
    return true;
}

bool TcpClient::IsOnline() const {
    return (_fd != INVALID_SOCKET);
}

void TcpClient::SetProtocol(IProtocol* proto) {
    _proto = proto;
}

void TcpClient::Shutdown() {
    if (!_shutdown) {
        _shutdown = true;
        _send_pending = false;

        _thd.Join();

        WSACloseEvent(_event);
        _event = WSA_INVALID_EVENT;

        raptor_set_socket_shutdown(_fd);
        _fd = INVALID_SOCKET;

        _s_mtx.Lock();
        _snd_buffer.ClearBuffer();
        _s_mtx.Unlock();

        _r_mtx.Lock();
        _rcv_buffer.ClearBuffer();
        _r_mtx.Unlock();
    }
}

void TcpClient::WorkThread(void*) {
    while (!_shutdown) {
        DWORD ret = WSAWaitForMultipleEvents(1, &_event, FALSE, 1000, FALSE);
        if (ret == WSA_WAIT_FAILED || ret == WSA_WAIT_TIMEOUT) {
            continue;
        }

        WSANETWORKEVENTS network_events;
        int r = WSAEnumNetworkEvents(_fd, _event, &network_events);
        if (r != 0) {
            continue;
        }

        int error = 0;
        if (network_events.lNetworkEvents & FD_CONNECT) {
            error = network_events.iErrorCode[FD_CONNECT_BIT];
            this->OnConnectEvent(error);
        }
        if (network_events.lNetworkEvents & FD_CLOSE) {
            error = network_events.iErrorCode[FD_CLOSE_BIT];
            this->OnCloseEvent(error);
        }
        if (network_events.lNetworkEvents & FD_READ) {
            error = network_events.iErrorCode[FD_READ_BIT];
            this->OnReadEvent(error);
        }
        if (network_events.lNetworkEvents & FD_WRITE) {
            error = network_events.iErrorCode[FD_WRITE_BIT];
            this->OnSendEvent(error);
        }
    }
}

void TcpClient::OnConnectEvent(int err) {
    if (err != 0) {
        _service->OnConnectResult(false);
        return;
    }

    DWORD transfered_bytes = 0;
    DWORD flags = 0;
    BOOL success = WSAGetOverlappedResult(_fd, &_conncet_overlapped, &transfered_bytes, FALSE, &flags);
    if (!success || !AsyncRecv()) {

        // cleanup
        Shutdown();

        _service->OnConnectResult(false);
    } else {
        _service->OnConnectResult(true);
    }
}

void TcpClient::OnCloseEvent(int) {
    // cleanup
    Shutdown();

    _service->OnClosed();
}

bool TcpClient::DoRecv() {
    DWORD transfered_bytes = 0;
    DWORD flags = 0;
    if (!WSAGetOverlappedResult(_fd, &_recv_overlapped, &transfered_bytes, FALSE, &flags)) {
        return false;
    }

    if (transfered_bytes == 0) {
        return false;
    }

    AutoMutex g(&_r_mtx);
    size_t node_size = _tmp_buffer[0].size();
    size_t count = transfered_bytes / node_size;
    size_t i = 0;

    for (; i < DEFAULT_TEMP_SLICE_COUNT && i < count; i++) {
        _rcv_buffer.AddSlice(_tmp_buffer[i]);
        _tmp_buffer[i] = MakeSliceByDefaultSize();
        transfered_bytes -= node_size;
    }

    if (transfered_bytes > 0) {
        Slice s(_tmp_buffer[i].Buffer(), transfered_bytes);
        _rcv_buffer.AddSlice(s);
    }

    if (ParsingProtocol() == -1) {
        return false;
    }
    return AsyncRecv();
}

void TcpClient::OnReadEvent(int err) {
    if (err != 0) {
        OnCloseEvent(err);
        return;
    }

    if (!DoRecv()) {
        OnCloseEvent(err);
    }
}

bool TcpClient::DoSend() {
    DWORD transfered_bytes = 0;
    DWORD flags = 0;

    _send_pending = false;

    if (!WSAGetOverlappedResult(_fd, &_send_overlapped, &transfered_bytes, FALSE, &flags)) {
        return false;
    }

    if (transfered_bytes == 0) {
        return false;
    }

    AutoMutex g(&_s_mtx);
    _snd_buffer.MoveHeader((size_t)transfered_bytes);
    if (!_snd_buffer.Empty()) {
        return AsyncSend();
    }
    return true;
}

void TcpClient::OnSendEvent(int err) {
    if (err != 0) {
        OnCloseEvent(err);
        return;
    }

    if (!DoSend()) {
        OnCloseEvent(err);
    }
}

constexpr size_t MAX_PACKAGE_SIZE = 0xffff;
constexpr size_t MAX_WSABUF_COUNT = 16;

bool TcpClient::AsyncSend() {
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
    int ret = WSASend(_fd, wsa_snd_buf, wsa_buf_count, NULL, 0, &_send_overlapped, NULL);

    if (ret == SOCKET_ERROR) {
        int error = WSAGetLastError();
        if (error != WSA_IO_PENDING) {
            _send_pending = false;
            return false;
        }
    }
    return true;
}

bool TcpClient::AsyncRecv() {
    WSABUF wsa_rcv_buf[DEFAULT_TEMP_SLICE_COUNT];
    DWORD dwFlag = 0;

    for (size_t i = 0; i < DEFAULT_TEMP_SLICE_COUNT; i++) {
        wsa_rcv_buf[i].buf = reinterpret_cast<char*>(_tmp_buffer[i].Buffer());
        wsa_rcv_buf[i].len = static_cast<ULONG>(_tmp_buffer[i].Length());
    }

    // https://docs.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-wsarecv
    int ret = WSARecv(_fd, wsa_rcv_buf, DEFAULT_TEMP_SLICE_COUNT, NULL, &dwFlag, &_recv_overlapped, NULL);

    if (ret == SOCKET_ERROR) {
        int error = WSAGetLastError();
        if (error != WSA_IO_PENDING) {
            return false;
        }
    }
    return true;
}

bool TcpClient::ReadSliceFromRecvBuffer(size_t read_size, Slice& s) {
    size_t cache_size = _rcv_buffer.GetBufferLength();
    if (read_size >= cache_size) {
        s = _rcv_buffer.Merge();
        return true;
    }
    s = _rcv_buffer.GetHeader(read_size);
    return false;
}

int TcpClient::ParsingProtocol() {
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
        }
        _service->OnMessageReceived(package.begin(), pack_len);
        _rcv_buffer.MoveHeader(pack_len);

        cache_size = _rcv_buffer.GetBufferLength();
        package_counter++;
    }
done:
    return package_counter;
}

} // namespace raptor
