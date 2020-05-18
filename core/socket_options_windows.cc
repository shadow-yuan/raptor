/*
 *
 * Copyright 2015 gRPC authors.
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

#include "core/socket_options.h"
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <limits.h>
#include <signal.h>
#include <string.h>

#include "core/sockaddr.h"
#include "core/socket_util.h"
#include "util/alloc.h"
#include "util/atomic.h"
#include "util/log.h"
#include "util/sync.h"
#include "util/string.h"


void raptor_global_socket_init() {
    WSADATA wsaData;
    int status = WSAStartup(MAKEWORD(2, 0), &wsaData);
    RAPTOR_ASSERT(status == 0);
}

void raptor_global_socket_shutdown() {
    int status = WSACleanup();
    RAPTOR_ASSERT(status == 0);
}

/* set a socket to non blocking mode */
raptor_error raptor_set_socket_nonblocking(raptor_socket_t rs, int non_blocking) {
    uint32_t param = 1;
    DWORD BytesReturned;
    int status = WSAIoctl(rs.fd, FIONBIO, &param, sizeof(param), NULL, 0, &BytesReturned,
                    NULL, NULL);
    return status == 0
                ? RAPTOR_ERROR_NONE
                : RAPTOR_WINDOWS_ERROR(WSAGetLastError(), "WSAIoctl(FIONBIO)");
}

raptor_error raptor_set_socket_cloexec(raptor_socket_t rs, int close_on_exec) {
    return RAPTOR_ERROR_NONE;
}

/* set a socket to reuse old addresses */
raptor_error raptor_set_socket_reuse_addr(raptor_socket_t rs, int reuse) {
    int val = (reuse) ? 1 : 0;
    int status = setsockopt(rs.fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&val, sizeof(val));

    return status == 0
            ? RAPTOR_ERROR_NONE
            : RAPTOR_WINDOWS_ERROR(WSAGetLastError(), "setsockopt(SO_REUSEADDR)");
}

/* disable nagle */
raptor_error raptor_set_socket_low_latency(raptor_socket_t rs, int low_latency) {
    int val = low_latency ? 1 : 0;
    int status = setsockopt(
            rs.fd, IPPROTO_TCP, TCP_NODELAY, (const char*)&val, sizeof(val));

    return status == 0
            ? RAPTOR_ERROR_NONE
            : RAPTOR_WINDOWS_ERROR(WSAGetLastError(), "setsockopt(TCP_NODELAY)");
}


/* set a socket to reuse old addresses */
raptor_error raptor_set_socket_reuse_port(raptor_socket_t rs, int reuse) {
    (void)rs;
    (void)reuse;
    return RAPTOR_ERROR_NONE;
}

raptor_error raptor_set_socket_snd_timeout(raptor_socket_t rs, int timeout_ms) {
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms - (tv.tv_sec * 1000)) * 1000;
    int status = setsockopt(rs.fd, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof(tv));

    return status == 0
            ? RAPTOR_ERROR_NONE
            : RAPTOR_WINDOWS_ERROR(WSAGetLastError(), "setsockopt(SO_SNDTIMEO)");
}

raptor_error raptor_set_socket_rcv_timeout(raptor_socket_t rs, int timeout_ms) {
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms - (tv.tv_sec * 1000)) * 1000;
    int status = setsockopt(rs.fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

    return status == 0
            ? RAPTOR_ERROR_NONE
            : RAPTOR_WINDOWS_ERROR(WSAGetLastError(), "setsockopt(SO_RCVTIMEO)");
}

raptor_error raptor_set_socket_ipv6_only(raptor_socket_t rs, int only) {
    int on_off = only ? 1 : 0;
    int status = setsockopt(
            rs.fd, IPPROTO_IPV6, IPV6_V6ONLY,
            (const char*)&on_off, sizeof(on_off));

    return status == 0
            ? RAPTOR_ERROR_NONE
            : RAPTOR_WINDOWS_ERROR(WSAGetLastError(), "setsockopt(IPV6_V6ONLY)");
}

void raptor_set_socket_shutdown(raptor_socket_t rs) {
    GUID guid = WSAID_DISCONNECTEX;
    LPFN_DISCONNECTEX DisconnectEx;
    DWORD ioctl_num_bytes;

    int status = WSAIoctl(rs.fd, SIO_GET_EXTENSION_FUNCTION_POINTER,
                    &guid, sizeof(guid), &DisconnectEx, sizeof(DisconnectEx),
                    &ioctl_num_bytes, NULL, NULL);

    if (status == 0) {
        DisconnectEx(rs.fd, NULL, 0, 0);
    } else {
        char* message = raptor_format_message(WSAGetLastError());
        log_info("Unable to retrieve DisconnectEx pointer : %s", message);
        raptor::Free(message);
    }
    closesocket(rs.fd);
}

raptor_error raptor_set_socket_tcp_user_timeout(raptor_socket_t rs, int timeout) {
    return RAPTOR_ERROR_NONE;
}

raptor_error raptor_set_socket_no_sigpipe_if_possible(raptor_socket_t rs) {
#ifdef SO_NOSIGPIPE
    int val = 1;
    int newval;
    socklen_t intlen = sizeof(newval);
    if (0 != setsockopt(rs.fd, SOL_SOCKET, SO_NOSIGPIPE, &val, sizeof(val))) {
        return RAPTOR_POSIX_ERROR("setsockopt(SO_NOSIGPIPE)");
    }
    if (0 != getsockopt(rs.fd, SOL_SOCKET, SO_NOSIGPIPE, &newval, &intlen)) {
        return RAPTOR_POSIX_ERROR("getsockopt(SO_NOSIGPIPE)");
    }
    if ((newval != 0) != (val != 0)) {
        return RAPTOR_ERROR_FROM_STATIC_STRING("Failed to set SO_NOSIGPIPE");
    }
#else
    (void)rs;
#endif
    return RAPTOR_ERROR_NONE;
}

static inline SOCKET ws2_socket(int family, int type, int protocol) {
    return WSASocket(family, type, protocol, NULL, 0, RAPTOR_WSA_SOCKET_FLAGS);
}

raptor_error raptor_create_dualstack_socket(
    const raptor_resolved_address* resolved_addr,
    int type, int protocol, raptor_dualstack_mode* dsmode, raptor_socket_t* newfd) {
    const raptor_sockaddr* addr =
        reinterpret_cast<const raptor_sockaddr*>(resolved_addr->addr);
    int family = addr->sa_family;
    if (family == AF_INET6) {
        newfd->fd = ws2_socket(family, type, protocol);

        /* Check if we've got a valid dualstack socket. */
        if (newfd->fd != INVALID_SOCKET && raptor_set_socket_ipv6_only(*newfd, 0) == RAPTOR_ERROR_NONE) {
            *dsmode = RAPTOR_DSMODE_DUALSTACK;
            return RAPTOR_ERROR_NONE;
        }
        /* If this isn't an IPv4 address, then return whatever we've got. */
        if (!raptor_sockaddr_is_v4mapped(resolved_addr, nullptr)) {
            *dsmode = RAPTOR_DSMODE_IPV6;
            if (newfd->fd == INVALID_SOCKET) {
                return RAPTOR_WINDOWS_ERROR(WSAGetLastError(), "WSASocket");
            }
            return RAPTOR_ERROR_NONE;
        }
        /* Fall back to AF_INET. */
        if (newfd->fd != INVALID_SOCKET) {
            closesocket(newfd->fd);
        }
        family = AF_INET;
    }
    *dsmode = (family == AF_INET) ? RAPTOR_DSMODE_IPV4 : RAPTOR_DSMODE_NONE;
    newfd->fd = ws2_socket(family, type, protocol);
    if (newfd->fd == INVALID_SOCKET) {
        return RAPTOR_WINDOWS_ERROR(WSAGetLastError(), "WSASocket");
    }
    return RAPTOR_ERROR_NONE;
}

raptor_error raptor_create_socket(
    const raptor_resolved_address* addr,
    raptor_socket_t* newfd, raptor_dualstack_mode* dsmode) {

    raptor_resolved_address addr6_v4mapped;
    raptor_resolved_address wildcard;
    int port = 0;

    if (raptor_sockaddr_to_v4mapped(addr, &addr6_v4mapped)) {
        addr = &addr6_v4mapped;
    }

    /* Treat :: or 0.0.0.0 as a family-agnostic wildcard. */
    if (raptor_sockaddr_is_wildcard(addr, &port)) {
        raptor_sockaddr_make_wildcard6(port, &wildcard);
        addr = &wildcard;
    }

    return raptor_create_dualstack_socket(addr, SOCK_STREAM, IPPROTO_TCP, dsmode, newfd);
}

raptor_error raptor_tcp_prepare_socket(raptor_socket_t sock) {
    raptor_error err;
    err = raptor_set_socket_nonblocking(sock, 1);
    if (err != RAPTOR_ERROR_NONE) return err;
    err = raptor_set_socket_ipv6_only(sock, 0);
    if (err != RAPTOR_ERROR_NONE) return err;
    err = raptor_set_socket_low_latency(sock, 1);
    if (err != RAPTOR_ERROR_NONE) return err;
    return RAPTOR_ERROR_NONE;
}

raptor_error raptor_tcp_server_prepare_socket(
    raptor_socket_t sock, const raptor_resolved_address* addr, int* port, int so_reuseport) {

    raptor_resolved_address sockname_temp;
    raptor_error error = RAPTOR_ERROR_NONE;
    int sockname_temp_len;

    (so_reuseport);

    error = raptor_tcp_prepare_socket(sock);
    if (error != RAPTOR_ERROR_NONE) {
        goto failure;
    }

    if (bind(sock.fd, (const raptor_sockaddr*)addr->addr, (int)addr->len) == SOCKET_ERROR) {
        error = RAPTOR_WINDOWS_ERROR(WSAGetLastError(), "bind");
        goto failure;
    }

    if (listen(sock.fd, SOMAXCONN) == SOCKET_ERROR) {
        error = RAPTOR_WINDOWS_ERROR(WSAGetLastError(), "listen");
        goto failure;
    }

    sockname_temp_len = sizeof(struct sockaddr_storage);
    if (getsockname(sock.fd, (raptor_sockaddr*)sockname_temp.addr,
                    &sockname_temp_len) == SOCKET_ERROR) {
        error = RAPTOR_WINDOWS_ERROR(WSAGetLastError(), "getsockname");
        goto failure;
    }
    sockname_temp.len = (size_t)sockname_temp_len;
    *port = raptor_sockaddr_get_port(&sockname_temp);
    return RAPTOR_ERROR_NONE;

failure:
    if (sock.fd != INVALID_SOCKET) {
        closesocket(sock.fd);
    }
    return error;
}
