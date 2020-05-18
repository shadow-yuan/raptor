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
#include "core/iocp/tcp_client.h"
#include "core/socket_util.h"
#include "util/log.h"

namespace raptor {
TcpClient::TcpClient(IRaptorClientEvent* service)
    : _service(service)
    , _progressing(false)
    , _shutdown(true)
    , _connectex(nullptr)
    , _fd(INVALID_SOCKET)
    , _event(WSA_INVALID_EVENT) {
}

TcpClient::~TcpClient() {}

raptor_error TcpClient::Init() {
    if (!_shutdown) {
        return RAPTOR_ERROR_FROM_STATIC_STRING("tcp client already running");
    }

    _event = WSACreateEvent();
    if (_event == WSA_INVALID_EVENT) {
        return RAPTOR_WSA_ERROR("WSACreateEvent");
    }

    _shutdown = false;
    _thd = Thread("client",
    [] (void* param) ->void {
        TcpClient* pthis = (TcpClient*)param;
        pthis->WorkThread();
    },
    this);

    _thd.Start();
    return RAPTOR_ERROR_NONE;
}

raptor_error TcpClient::Connect(const std::string& addr, size_t timeout_ms) {
    if (_shutdown) {
        return RAPTOR_ERROR_FROM_STATIC_STRING("TcpClient is not initialized");
    }
    if (addr.empty()) {
        return RAPTOR_ERROR_FROM_STATIC_STRING("Invalid parameter");
    }
    if (_progressing) {
        return RAPTOR_ERROR_FROM_STATIC_STRING("Connection operation in progress");
    }
    raptor_resolved_addresses* addrs;
    auto e = raptor_blocking_resolve_address(addr.c_str(), nullptr, &addrs);
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
            WSAIoctl(_fd, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid, sizeof(guid),
                    &_connectex, sizeof(_connectex), &ioctl_num_bytes, NULL, NULL);

        if (status != 0) {
            return RAPTOR_WSA_ERROR("WSAIoctl(SIO_GET_EXTENSION_FUNCTION_POINTER)");
        }
    }
    return RAPTOR_ERROR_NONE;
}

raptor_error TcpClient::InternalConnect(const raptor_resolved_address* addr) {
    raptor_error error;
    raptor_dualstack_mode mode;
    raptor_resolved_address local_address;
    int status;
    BOOL ret;

    error = raptor_create_socket(addr, &_fd, &mode);
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
        error = RAPTOR_WSA_ERROR("bind");
        goto failure;
    }

    /* Associate event type FD_CONNECT with the fd and event */
    WSAEventSelect(_fd, _event, FD_CONNECT);

    ret = _connectex(_fd,
                    (raptor_sockaddr*)&addr->addr, (int)addr->len,
                    NULL, 0, NULL, &_c_overlapped);

    /* It wouldn't be unusual to get a success immediately. But we'll still get
        an IOCP notification, so let's ignore it. */
    if (!ret) {
        int last_error = WSAGetLastError();
        if (last_error != ERROR_IO_PENDING) {
            error = RAPTOR_WSA_ERROR("ConnectEx");
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

raptor_error TcpClient::Send(const void* buff, size_t len) {
    _s_mtx.Lock();
    _snd_buffer.AddSlice(Slice(buff, len));
    Slice s = _snd_buffer.GetTopSlice();
    AsyncSend(&s);
    _s_mtx.Unlock();
    return RAPTOR_ERROR_NONE;
}

raptor_error TcpClient::Shutdown() {
    if (!_shutdown) {
        _shutdown = false;
        _thd.Join();

        WSACloseEvent(_event);
        _event = WSA_INVALID_EVENT;

        raptor_set_socket_shutdown(_fd);
        _fd = INVALID_SOCKET;
        return RAPTOR_ERROR_NONE;
    }
    return RAPTOR_ERROR_FROM_STATIC_STRING("tcp client not running");
}

void TcpClient::WorkThread() {
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
            this->OnConnectedEvent(error);
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

void TcpClient::OnConnectedEvent(int err) {
    DWORD transfered_bytes = 0;
    DWORD flags = 0;
    BOOL success = WSAGetOverlappedResult(_fd, &_c_overlapped, &transfered_bytes, FALSE, &flags);
    if (!success) {
        closesocket(_fd);
        // TODO(SHADOW): clean up
        _service->OnConnectResult(false);
    } else {
        _service->OnConnectResult(true);
    }
}

void TcpClient::OnCloseEvent(int err) {
    // TODO(SHADOW): clean up
    closesocket(_fd);
    _service->OnClosed();
}

void TcpClient::OnReadEvent(int err) {
    DWORD transfered_bytes = 0;
    DWORD flags = 0;
    BOOL success = WSAGetOverlappedResult(_fd, &_r_overlapped, &transfered_bytes, FALSE, &flags);
    if (!success) {
        OnCloseEvent(err);
        return;
    }


}

void TcpClient::OnSendEvent(int err) {
    DWORD transfered_bytes = 0;
    DWORD flags = 0;
    BOOL success = WSAGetOverlappedResult(_fd, &_s_overlapped, &transfered_bytes, FALSE, &flags);
    if (!success) {
        OnCloseEvent(err);
        return;
    }

    _s_mtx.Lock();
    _snd_buffer.MoveHeader((size_t)transfered_bytes);
    if (_snd_buffer.GetBufferLength() > 0) {
        Slice s = _snd_buffer.GetTopSlice();
        AsyncSend(&s);
    }
    _s_mtx.Unlock();
}

bool TcpClient::AsyncSend(Slice* s) {

    _wsa_snd_buf.buf = (char*)s->Buffer();
    _wsa_snd_buf.len = s->Length();

    // https://docs.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-wsasend
    int ret = WSASend(_fd, &_wsa_snd_buf, 1, NULL, 0, &_s_overlapped, NULL);

    if (ret == SOCKET_ERROR) {
        int error = WSAGetLastError();
        if (error != WSA_IO_PENDING) {
            return false;
        }
    }
    return true;
}
} // namespace raptor
