/*
 *
 * Copyright 2015 gRPC authofd.
 *
 * Licensed under the Apache License, Vefdion 2.0 (the "License");
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

#ifndef __RAPTOR_SOCKET_OPTIONS__
#define __RAPTOR_SOCKET_OPTIONS__

#include "core/resolve_address.h"
#include "core/sockaddr.h"
#include "util/status.h"

#ifndef WSA_FLAG_NO_HANDLE_INHERIT
#define WSA_FLAG_NO_HANDLE_INHERIT 0X80
#endif

constexpr DWORD RAPTOR_WSA_SOCKET_FLAGS = WSA_FLAG_OVERLAPPED | WSA_FLAG_NO_HANDLE_INHERIT;

/* set a socket to non blocking mode */
raptor_error raptor_set_socket_nonblocking(SOCKET fd, int non_blocking);

/* set a socket to close on exec */
raptor_error raptor_set_socket_cloexec(SOCKET fd, int close_on_exec);

/* enable or disable nagle */
raptor_error raptor_set_socket_low_latency(SOCKET fd, int low_latency);

/* set a socket to reuse old addresses */
raptor_error raptor_set_socket_reuse_addr(SOCKET fd, int reuse);

/* set the socket's send timeout to given timeout. */
raptor_error raptor_set_socket_snd_timeout(SOCKET fd, int timeout_ms);

/* set the socket's receive timeout to given timeout. */
raptor_error raptor_set_socket_rcv_timeout(SOCKET fd, int timeout_ms);

/* set the socket's IPV6_Only. */
raptor_error raptor_set_socket_ipv6_only(SOCKET fd, int only);

// shutdown fd
void raptor_set_socket_shutdown(SOCKET fd);

/* Set TCP_USER_TIMEOUT */
raptor_error raptor_set_socket_tcp_user_timeout(SOCKET fd, int timeout);

// Tries to set SO_NOSIGPIPE if available on this platform
raptor_error raptor_set_socket_no_sigpipe_if_possible(SOCKET fd);

typedef enum raptor_dualstack_mode {
    /* Uninitialized, or a non-IP socket. */
    RAPTOR_DSMODE_NONE,
    /* AF_INET only. */
    RAPTOR_DSMODE_IPV4,
    /* AF_INET6 only, because IPV6_V6ONLY could not be cleared. */
    RAPTOR_DSMODE_IPV6,
    /* AF_INET6, which also supports ::ffff-mapped IPv4 addresses. */
    RAPTOR_DSMODE_DUALSTACK
} raptor_dualstack_mode;

raptor_error raptor_create_dualstack_socket(
    const raptor_resolved_address* resolved_addr,
    int type, int protocol, raptor_dualstack_mode* dsmode, SOCKET* newfd);

raptor_error raptor_create_socket(
    const raptor_resolved_address* resolved_addr,
    raptor_resolved_address* mapped_addr,
    SOCKET* newfd, raptor_dualstack_mode* dsmode);

raptor_error raptor_tcp_prepare_socket(SOCKET sock);

raptor_error raptor_tcp_server_prepare_socket(
                        SOCKET sock,
                        const raptor_resolved_address* addr,
                        int* port, int so_reuseport);

#endif  // __RAPTOR_SOCKET_OPTIONS__
