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
#include "core/socket_util.h"
#include "raptor/protocol.h"
#include "util/alloc.h"
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

    _fd = fd;

    _rcv_thd = r;
    _snd_thd = s;

    _rcv_thd->Add(fd, (void*)_cid, EPOLLIN | EPOLLET);
    _snd_thd->Add(fd, (void*)_cid, EPOLLOUT | EPOLLET);

    _addr = *addr;

    char* output = nullptr;
    int bytes = raptor_sockaddr_to_string(&output, &_addr, 1);
    if (output && bytes > 0) {
        _addr_str = Slice(output, static_cast<size_t>(bytes+1));
    }
    if (output) {
        Free(output);
    }
    _service->OnConnectionArrived(_cid, &_addr_str);
}

void Connection::SetProtocol(IProtocol* p) {
    _proto = p;
}

bool Connection::SendWithHeader(const void* hdr, size_t hdr_len, const void* data, size_t data_len) {
    if (!IsOnline()) return false;
    AutoMutex g(&_snd_mutex);
    if (hdr != nullptr && hdr_len > 0) {
        _snd_buffer.AddSlice(Slice(hdr, hdr_len));
    }
    if (data != nullptr && data_len > 0) {
        _snd_buffer.AddSlice(Slice(data, data_len));
    }
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
    return false;
}

bool Connection::DoSendEvent() {
    int result = OnSend();
    if (result == 0) {
        return true;
    }
    return false;
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
        if (ParsingProtocol() == -1) {
            return -1;
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
