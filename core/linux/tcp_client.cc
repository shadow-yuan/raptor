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
#include "core/linux/tcp_client.h"
#include <sys/select.h>
#include "core/socket_util.h"
#include "util/log.h"
#include "core/linux/socket_setting.h"

namespace raptor {
TcpClient::TcpClient(IClientReceiver* service)
    : _service(service)
    , _proto(nullptr)
    , _shutdown(true)
    , _fd(-1) {
}

TcpClient::~TcpClient() {}

raptor_error TcpClient::Init() {
    if (!_shutdown) {
        return RAPTOR_ERROR_FROM_STATIC_STRING("tcp client already running");
    }

    _shutdown = false;
    _is_connected = false;

    _thd = Thread("client",
        std::bind(&TcpClient::WorkThread, this, std::placeholders::_1), nullptr);

    _thd.Start();
    return RAPTOR_ERROR_NONE;
}

raptor_error TcpClient::Connect(const char* addr, size_t timeout_ms) {
    if (_shutdown) {
        return RAPTOR_ERROR_FROM_STATIC_STRING("TcpClient is not initialized");
    }

    raptor_resolved_addresses* addrs;
    auto e = raptor_blocking_resolve_address(addr, nullptr, &addrs);
    if (e != RAPTOR_ERROR_NONE) {
        return e;
    }
    RAPTOR_ASSERT(addrs->naddrs > 0);
    e = AsyncConnect(&addrs->addrs[0], static_cast<int>(timeout_ms), &_fd);
    raptor_resolved_addresses_destroy(addrs);
    return e;
}

bool TcpClient::Send(const void* buff, size_t len) {
    if (!IsOnline()) {
        return false;
    }

    AutoMutex g(&_s_mtx);
    _snd_buffer.AddSlice(Slice(buff, len));
    return true;
}

bool TcpClient::IsOnline() const {
    return (_fd != -1);
}

void TcpClient::SetProtocol(IProtocol* proto) {
    _proto = proto;
}

void TcpClient::Shutdown() {
    if (!_shutdown) {
        _shutdown = true;

        _thd.Join();

        raptor_set_socket_shutdown(_fd);
        _fd = -1;

        _s_mtx.Lock();
        _snd_buffer.ClearBuffer();
        _s_mtx.Unlock();

        _r_mtx.Lock();
        _rcv_buffer.ClearBuffer();
        _r_mtx.Unlock();
    }
}

void TcpClient::WorkThread(void* ptr) {
    bool error_occurred = false;
    while (!_shutdown) {

        fd_set rfs, wfs;
        FD_ZERO(&rfs);
        FD_SET(_fd, &rfs);
        FD_ZERO(&wfs);
        FD_SET(_fd, &wfs);

        struct timeval t;
        t.tv_sec = 1;
        t.tv_usec = 0;

        int r = select(_fd+1, &rfs, &wfs, NULL, &t);
        if (r <= 0) {
            if (errno == EINTR || errno == EWOULDBLOCK || errno == EAGAIN) {
                continue;
            }
            error_occurred = true;
            break;
        }

        if (!_is_connected) {
            _is_connected = true;
            _service->OnConnectResult(true);
            continue;
        }

        if (FD_ISSET(_fd, &rfs)) {
            error_occurred = (DoRecv() == 0);
        } else {
            error_occurred = (DoSend() == 0);
        }

        if (!error_occurred) {
            break;
        }
    }

    if (_is_connected) {
        _service->OnClosed();
    } else {
        _service->OnConnectResult(false);
    }
}


int TcpClient::DoRecv() {
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
        if (ParsingProtocol() == -1) {
            return -1;
        }

    } while (recv_bytes == unused_space);
    return 0;
}

int TcpClient::DoSend() {
    if (_snd_buffer.Empty()) {
        return 0;
    }

    size_t count = 0;
    do {
        Slice slice = _snd_buffer.GetTopSlice();

        int slen = ::send(_fd, slice.begin(), slice.size(), 0);

        if (slen > 0) {
            _snd_buffer.MoveHeader(slice.size());
        } else if (slen < 0) {
            if (errno == EINTR || errno == EWOULDBLOCK || errno == EAGAIN) {
                return -1;
            }
        } else {
            break;
        }
        count = _snd_buffer.Count();
    } while (count > 0);
    return 0;
}

raptor_error TcpClient::AsyncConnect(
    const raptor_resolved_address* addr, int timeout_ms, int* new_fd) {
    raptor_resolved_address mapped_addr;
    int sock_fd = -1;

    *new_fd = -1;

    raptor_error result = raptor_tcp_client_prepare_socket(addr, &mapped_addr, &sock_fd, timeout_ms);
    if (result != RAPTOR_ERROR_NONE) {
        return result;
    }
    int err = 0;
    do {
        err = connect(sock_fd, (const raptor_sockaddr*)mapped_addr.addr, mapped_addr.len);
    } while (err < 0 && errno == EINTR);

    if (errno != EWOULDBLOCK && errno != EINPROGRESS) {
        raptor_set_socket_shutdown(sock_fd);
        return RAPTOR_POSIX_ERROR("connect");
    }
    *new_fd = sock_fd;
    return RAPTOR_ERROR_NONE;
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
