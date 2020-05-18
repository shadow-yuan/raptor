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
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include "util/alloc.h"
#include "util/log.h"
#include "util/sync.h"
#include "util/string.h"

void raptor_global_socket_init() {
#ifndef SO_NOSIGPIPE
    signal(SIGPIPE, SIG_IGN);
#endif
}

void raptor_global_socket_shutdown() {
#ifndef SO_NOSIGPIPE
    signal(SIGPIPE, SIG_DFL);
#endif
}

/* set a socket to non blocking mode */
raptor_error raptor_set_socket_nonblocking(raptor_socket_t rs, int non_blocking) {
    int oldflags = fcntl(rs.fd, F_GETFL, 0);
    if (oldflags < 0) {
        return RAPTOR_POSIX_ERROR("fcntl");
    }

    if (non_blocking) {
        oldflags |= O_NONBLOCK;
    } else {
        oldflags &= ~O_NONBLOCK;
    }

    if (fcntl(fd, F_SETFL, oldflags) != 0) {
        return RAPTOR_POSIX_ERROR("fcntl");
    }

    return RAPTOR_ERROR_NONE;
}

/* set a socket to close on exec */
raptor_error raptor_set_socket_cloexec(raptor_socket_t rs, int close_on_exec) {
    int oldflags = fcntl(rs.fd, F_GETFD, 0);
    if (oldflags < 0) {
        return RAPTOR_POSIX_ERROR("fcntl");
    }

    if (close_on_exec) {
        oldflags |= FD_CLOEXEC;
    } else {
        oldflags &= ~FD_CLOEXEC;
    }

    if (fcntl(fd, F_SETFD, oldflags) != 0) {
        return RAPTOR_POSIX_ERROR("fcntl");
    }

    return RAPTOR_ERROR_NONE;
}

/* set a socket to reuse old addresses */
raptor_error raptor_set_socket_reuse_addr(raptor_socket_t rs, int reuse) {
    int val = (reuse != 0);
    int newval;
    socklen_t intlen = sizeof(newval);
    if (0 != setsockopt(rs.fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val))) {
        return RAPTOR_POSIX_ERROR("setsockopt(SO_REUSEADDR)");
    }
    if (0 != getsockopt(rs.fd, SOL_SOCKET, SO_REUSEADDR, &newval, &intlen)) {
        return RAPTOR_POSIX_ERROR("getsockopt(SO_REUSEADDR)");
    }
    if ((newval != 0) != val) {
        return RAPTOR_ERROR_FROM_STATIC_STRING("Failed to set SO_REUSEADDR");
    }
    return RAPTOR_ERROR_NONE;
}

/* disable nagle */
raptor_error raptor_set_socket_low_latency(raptor_socket_t rs, int low_latency) {
    int val = (low_latency != 0);
    int newval;
    socklen_t intlen = sizeof(newval);
    if (0 != setsockopt(rs.fd, IPPROTO_TCP, TCP_NODELAY, &val, sizeof(val))) {
        return RAPTOR_POSIX_ERROR("setsockopt(TCP_NODELAY)");
    }
    if (0 != getsockopt(rs.fd, IPPROTO_TCP, TCP_NODELAY, &newval, &intlen)) {
        return RAPTOR_POSIX_ERROR("getsockopt(TCP_NODELAY)");
    }
    if ((newval != 0) != val) {
        return RAPTOR_ERROR_FROM_STATIC_STRING("Failed to set TCP_NODELAY");
    }
    return RAPTOR_ERROR_NONE;
}

/* set a socket to reuse old addresses */
raptor_error raptor_set_socket_reuse_port(raptor_socket_t rs, int reuse) {
#ifdef SO_REUSEPORT
    int val = (reuse != 0) ? 1: 0;
    int newval;
    socklen_t intlen = sizeof(newval);
    if (0 != setsockopt(rs.fd, SOL_SOCKET, SO_REUSEPORT, &val, sizeof(val))) {
        return RAPTOR_POSIX_ERROR("setsockopt(SO_REUSEPORT)");
    }
    if (0 != getsockopt(rs.fd, SOL_SOCKET, SO_REUSEPORT, &newval, &intlen)) {
        return RAPTOR_POSIX_ERROR("getsockopt(SO_REUSEPORT)");
    }
    if ((newval != 0) != val) {
        return RAPTOR_ERROR_FROM_STATIC_STRING("Failed to set SO_REUSEPORT");
    }
#else
    (void)fd;
    (void)reuse;
#endif
    return RAPTOR_ERROR_NONE;
}

raptor_error raptor_set_socket_snd_timeout(raptor_socket_t rs, int timeout_ms) {
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms - (tv.tv_sec * 1000)) * 1000;
    int status = setsockopt(rs.fd, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof(tv));

    return status == 0
            ? RAPTOR_ERROR_NONE
            : RAPTOR_POSIX_ERROR("setsockopt(SO_SNDTIMEO)");
}

raptor_error raptor_set_socket_rcv_timeout(raptor_socket_t rs, int timeout_ms) {
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms - (tv.tv_sec * 1000)) * 1000;
    int status = setsockopt(rs.fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

    return status == 0
            ? RAPTOR_ERROR_NONE
            : RAPTOR_POSIX_ERROR("setsockopt(SO_RCVTIMEO)");
}

raptor_error raptor_set_socket_ipv6_only(raptor_socket_t rs, int only) {
    int on_off = only ? 1 : 0;
    int status = setsockopt(
            rs.fd, IPPROTO_IPV6, IPV6_V6ONLY,
            &on_off, sizeof(on_off));

    return status == 0
            ? RAPTOR_ERROR_NONE
            : RAPTOR_POSIX_ERROR("setsockopt(IPV6_V6ONLY)");
}

void raptor_set_socket_shutdown(raptor_socket_t rs) {
    if (rs.fd < 0) return 0;
    // close the read and write on the socket
    shutdown(rs.fd, SHUT_RDWR);
    close(rs.fd);
}

static int raptor_is_unix_socket(const raptor_resolved_address* resolved_addr) {
    const raptor_sockaddr* addr =
        reinterpret_cast<const raptor_sockaddr*>(resolved_addr->addr);
    return addr->sa_family == AF_UNIX;
}

raptor_error raptor_set_socket_tcp_user_timeout(raptor_socket_t rs, int timeout) {
    int newval;
    socklen_t len = sizeof(newval);
    if (0 != setsockopt(rs.fd, IPPROTO_TCP, TCP_USER_TIMEOUT, &timeout,
                        sizeof(timeout))) {
        log_error("setsockopt(TCP_USER_TIMEOUT) %s", strerror(errno));
        return RAPTOR_ERROR_NONE;
    }
    if (0 != getsockopt(rs.fd, IPPROTO_TCP, TCP_USER_TIMEOUT, &newval, &len)) {
        log_error("getsockopt(TCP_USER_TIMEOUT) %s", strerror(errno));
        return RAPTOR_ERROR_NONE;
    }

    if (newval != timeout) {
        /* Do not fail on failing to set TCP_USER_TIMEOUT for now. */
        log_error("Failed to set TCP_USER_TIMEOUT");
    }
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
    (void)fd;
#endif
    return RAPTOR_ERROR_NONE;
}

raptor_error raptor_create_dualstack_socket(
    const raptor_resolved_address* resolved_addr,
    int type, int protocol, raptor_dualstack_mode* dsmode, raptor_socket_t* newfd) {
    const raptor_sockaddr* addr =
        reinterpret_cast<const raptor_sockaddr*>(resolved_addr->addr);
    int family = addr->sa_family;
    if (family == AF_INET6) {
        newfd->fd = socket(family, type, protocol);
        /* Check if we've got a valid dualstack socket. */
        if (newfd->fd >= 0 && raptor_set_socket_ipv6_only(*newfd, 0)) {
            *dsmode = RAPTOR_DSMODE_DUALSTACK;
            return RAPTOR_ERROR_NONE;
        }
        /* If this isn't an IPv4 address, then return whatever we've got. */
        if (!raptor_sockaddr_is_v4mapped(resolved_addr, nullptr)) {
            *dsmode = RAPTOR_DSMODE_IPV6;
            if (newfd->fd <= 0) {
                return RAPTOR_POSIX_ERROR("socket");
            }
            return RAPTOR_ERROR_NONE;
        }
        /* Fall back to AF_INET. */
        if (newfd->fd >= 0) {
            close(newfd->fd);
        }
        family = AF_INET;
    }
    *dsmode = family == AF_INET ? RAPTOR_DSMODE_IPV4 : RAPTOR_DSMODE_NONE;
    newfd->fd = socket(family, type, protocol);
    if (newfd->fd <= 0) {
        return RAPTOR_POSIX_ERROR("socket");
    }
    return RAPTOR_ERROR_NONE;
}

// -----------------------------------------------------------
/* get max listen queue size on linux */
#define MIN_SAFE_ACCEPT_QUEUE_SIZE 100
static raptor_once_t s_init_max_accept_queue_size = RAPTOR_ONCE_INIT;
static int s_max_accept_queue_size = 0;
static void init_max_accept_queue_size(void) {
    int n = SOMAXCONN;
    char buf[64];
    FILE* fp = fopen("/proc/sys/net/core/somaxconn", "r");
    if (fp == nullptr) {
        /* 2.4 kernel. */
        s_max_accept_queue_size = SOMAXCONN;
        return;
    }
    if (fgets(buf, sizeof buf, fp)) {
        char* end;
        long i = strtol(buf, &end, 10);
        if (i > 0 && i <= INT_MAX && end && *end == '\n') {
            n = static_cast<int>(i);
        }
    }
    fclose(fp);
    s_max_accept_queue_size = n;

    if (s_max_accept_queue_size < MIN_SAFE_ACCEPT_QUEUE_SIZE) {
    log_info(
            "suspiciously small accept queue (%d) will probably lead to "
            "connection drops",
            s_max_accept_queue_size);
    }
}

static int get_max_accept_queue_size(void) {
    pthread_once(&s_init_max_accept_queue_size, init_max_accept_queue_size);
    return s_max_accept_queue_size;
}

/* Prepare a recently-created socket for listening. */
raptor_error raptor_tcp_server_prepare_socket(
    raptor_socket_t sock, const raptor_resolved_address* addr,
    int* port, int so_reuseport) {

    raptor_resolved_address sockname_temp;
    raptor_error err = RAPTOR_ERROR_NONE;

    RAPTOR_ASSERT(sock.fd >= 0);

    if (so_reuseport && !raptor_is_unix_socket(addr)) {
        err = raptor_set_socket_reuse_port(sock, 1);
        if (err != RAPTOR_ERROR_NONE) goto error;
    }

    err = raptor_set_socket_nonblocking(sock, 1);
    if (err != RAPTOR_ERROR_NONE) goto error;
    err = raptor_set_socket_cloexec(sock, 1);
    if (err != RAPTOR_ERROR_NONE) goto error;
    if (!raptor_is_unix_socket(addr)) {
        err = raptor_set_socket_low_latency(sock, 1);
        if (err != RAPTOR_ERROR_NONE) goto error;
        err = raptor_set_socket_reuse_addr(sock, 1);
        if (err != RAPTOR_ERROR_NONE) goto error;
        err = raptor_set_socket_tcp_user_timeout(fd, DEFAULT_SERVER_TCP_USER_TIMEOUT_MS);
        if (err != RAPTOR_ERROR_NONE) goto error;
    }
    // setsockopt SO_NOSIGPIPE
    err = raptor_set_socket_no_sigpipe_if_possible(fd);
    if (err != RAPTOR_ERROR_NONE) goto error;

    if (bind(sock.fd, reinterpret_cast<raptor_sockaddr*>(const_cast<char*>(addr->addr)),
            addr->len) < 0) {
        err = RAPTOR_POSIX_ERROR("bind");
        goto error;
    }

    if (listen(sock.fd, get_max_accept_queue_size()) < 0) {
        err = RAPTOR_POSIX_ERROR("listen");
        goto error;
    }

    sockname_temp.len = static_cast<socklen_t>(sizeof(struct sockaddr_storage));

    if (getsockname(sock.fd, reinterpret_cast<raptor_sockaddr*>(sockname_temp.addr),
                    &sockname_temp.len) < 0) {
        err = RAPTOR_POSIX_ERROR("getsockname");
        goto error;
    }

    *port = raptor_sockaddr_get_port(&sockname_temp);
    return RAPTOR_ERROR_NONE;

error:
    RAPTOR_ASSERT(err != RAPTOR_ERROR_NONE);
    if (fd >= 0) {
        close(fd);
    }

    return err;
}
