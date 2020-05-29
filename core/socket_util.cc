/*
 *
 * Copyright 2016 gRPC authors.
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

#include "core/socket_util.h"
#include <stdint.h>
#include <string.h>

#include "core/host_port.h"
#include "core/resolve_address.h"
#include "core/sockaddr.h"

#include "util/log.h"
#include "util/string_view.h"

#ifdef _WIN32
const char* raptor_inet_ntop(int af, const void* src, char* dst, size_t size) {
    /* Windows InetNtopA wants a mutable ip pointer */
    return InetNtopA(af, (void*)src, dst, size);
}
#else
const char* raptor_inet_ntop(int af, const void* src, char* dst, size_t size) {
    RAPTOR_ASSERT(size <= (socklen_t)-1);
    return inet_ntop(af, src, dst, static_cast<socklen_t>(size));
}
#endif

static const uint8_t kV4MappedPrefix[] = {0, 0, 0, 0, 0,    0,
                                          0, 0, 0, 0, 0xff, 0xff};

int raptor_sockaddr_is_v4mapped(const raptor_resolved_address* resolved_addr,
                                raptor_resolved_address* resolved_addr4_out) {
    RAPTOR_ASSERT(resolved_addr != resolved_addr4_out);
    const raptor_sockaddr* addr =
        reinterpret_cast<const raptor_sockaddr*>(resolved_addr->addr);
    raptor_sockaddr_in* addr4_out =
        resolved_addr4_out == nullptr
            ? nullptr
            : reinterpret_cast<raptor_sockaddr_in*>(resolved_addr4_out->addr);
    if (addr->sa_family == AF_INET6) {
        const raptor_sockaddr_in6* addr6 =
            reinterpret_cast<const raptor_sockaddr_in6*>(addr);
        if (memcmp(addr6->sin6_addr.s6_addr, kV4MappedPrefix,
                    sizeof(kV4MappedPrefix)) == 0) {
            if (resolved_addr4_out != nullptr) {
                /* Normalize ::ffff:0.0.0.0/96 to IPv4. */
                memset(resolved_addr4_out, 0, sizeof(*resolved_addr4_out));
                addr4_out->sin_family = AF_INET;
                /* s6_addr32 would be nice, but it's non-standard. */
                memcpy(&addr4_out->sin_addr, &addr6->sin6_addr.s6_addr[12], 4);
                addr4_out->sin_port = addr6->sin6_port;
                resolved_addr4_out->len =
                    static_cast<socklen_t>(sizeof(raptor_sockaddr_in));
            }
            return 1;
        }
    }
    return 0;
}

int raptor_sockaddr_to_v4mapped(const raptor_resolved_address* resolved_addr,
                                raptor_resolved_address* resolved_addr6_out) {
    RAPTOR_ASSERT(resolved_addr != resolved_addr6_out);
    const raptor_sockaddr* addr =
        reinterpret_cast<const raptor_sockaddr*>(resolved_addr->addr);
    raptor_sockaddr_in6* addr6_out =
        reinterpret_cast<raptor_sockaddr_in6*>(resolved_addr6_out->addr);
    if (addr->sa_family == AF_INET) {
        const raptor_sockaddr_in* addr4 =
            reinterpret_cast<const raptor_sockaddr_in*>(addr);
        memset(resolved_addr6_out, 0, sizeof(*resolved_addr6_out));
        addr6_out->sin6_family = AF_INET6;
        memcpy(&addr6_out->sin6_addr.s6_addr[0], kV4MappedPrefix, 12);
        memcpy(&addr6_out->sin6_addr.s6_addr[12], &addr4->sin_addr, 4);
        addr6_out->sin6_port = addr4->sin_port;
        resolved_addr6_out->len = static_cast<socklen_t>(sizeof(raptor_sockaddr_in6));
        return 1;
    }
    return 0;
}

int raptor_sockaddr_is_wildcard(const raptor_resolved_address* resolved_addr,
                                int* port_out) {
    const raptor_sockaddr* addr;
    raptor_resolved_address addr4_normalized;
    if (raptor_sockaddr_is_v4mapped(resolved_addr, &addr4_normalized)) {
        resolved_addr = &addr4_normalized;
    }
    addr = reinterpret_cast<const raptor_sockaddr*>(resolved_addr->addr);
    if (addr->sa_family == AF_INET) {
        /* Check for 0.0.0.0 */
        const raptor_sockaddr_in* addr4 =
            reinterpret_cast<const raptor_sockaddr_in*>(addr);
        if (addr4->sin_addr.s_addr != 0) {
            return 0;
        }
        *port_out = ntohs(addr4->sin_port);
        return 1;
    } else if (addr->sa_family == AF_INET6) {
        /* Check for :: */
        const raptor_sockaddr_in6* addr6 =
            reinterpret_cast<const raptor_sockaddr_in6*>(addr);
        int i;
        for (i = 0; i < 16; i++) {
            if (addr6->sin6_addr.s6_addr[i] != 0) {
            return 0;
            }
        }
        *port_out = ntohs(addr6->sin6_port);
        return 1;
    } else {
        return 0;
    }
}

void grpc_sockaddr_make_wildcards(int port, raptor_resolved_address* wild4_out,
                                  raptor_resolved_address* wild6_out) {
    raptor_sockaddr_make_wildcard4(port, wild4_out);
    raptor_sockaddr_make_wildcard6(port, wild6_out);
}

void raptor_sockaddr_make_wildcard4(int port,
                                    raptor_resolved_address* resolved_wild_out) {
    raptor_sockaddr_in* wild_out =
        reinterpret_cast<raptor_sockaddr_in*>(resolved_wild_out->addr);
    RAPTOR_ASSERT(port >= 0 && port < 65536);
    memset(resolved_wild_out, 0, sizeof(*resolved_wild_out));
    wild_out->sin_family = AF_INET;
    wild_out->sin_port = htons(static_cast<uint16_t>(port));
    resolved_wild_out->len = static_cast<socklen_t>(sizeof(raptor_sockaddr_in));
}

void raptor_sockaddr_make_wildcard6(int port,
                                    raptor_resolved_address* resolved_wild_out) {
    raptor_sockaddr_in6* wild_out =
        reinterpret_cast<raptor_sockaddr_in6*>(resolved_wild_out->addr);
    RAPTOR_ASSERT(port >= 0 && port < 65536);
    memset(resolved_wild_out, 0, sizeof(*resolved_wild_out));
    wild_out->sin6_family = AF_INET6;
    wild_out->sin6_port = htons(static_cast<uint16_t>(port));
    resolved_wild_out->len = static_cast<socklen_t>(sizeof(raptor_sockaddr_in6));
}

int raptor_sockaddr_to_string(char** out,
                              const raptor_resolved_address* resolved_addr,
                              int normalize) {
    const raptor_sockaddr* addr;
    const int save_errno = errno;
    raptor_resolved_address addr_normalized;
    char ntop_buf[INET6_ADDRSTRLEN];
    const void* ip = nullptr;
    int port = 0;
    uint32_t sin6_scope_id = 0;
    int ret;

    *out = nullptr;
    if (normalize && raptor_sockaddr_is_v4mapped(resolved_addr, &addr_normalized)) {
        resolved_addr = &addr_normalized;
    }
    addr = reinterpret_cast<const raptor_sockaddr*>(resolved_addr->addr);
    if (addr->sa_family == AF_INET) {
        const raptor_sockaddr_in* addr4 =
            reinterpret_cast<const raptor_sockaddr_in*>(addr);
        ip = &addr4->sin_addr;
        port = ntohs(addr4->sin_port);
    } else if (addr->sa_family == AF_INET6) {
        const raptor_sockaddr_in6* addr6 =
            reinterpret_cast<const raptor_sockaddr_in6*>(addr);
        ip = &addr6->sin6_addr;
        port = ntohs(addr6->sin6_port);
        sin6_scope_id = addr6->sin6_scope_id;
    }
    if (ip != nullptr && raptor_inet_ntop(addr->sa_family, ip, ntop_buf,
                                        sizeof(ntop_buf)) != nullptr) {
        raptor::UniquePtr<char> tmp_out;
        if (sin6_scope_id != 0) {
            char* host_with_scope;
            /* Enclose sin6_scope_id with the format defined in RFC 6784 section 2. */
            raptor_asprintf(&host_with_scope, "%s%%25%u", ntop_buf, sin6_scope_id);
            ret = raptor::JoinHostPort(&tmp_out, host_with_scope, port);
            raptor::Free(host_with_scope);
        } else {
            ret = raptor::JoinHostPort(&tmp_out, ntop_buf, port);
        }
        *out = tmp_out.release();
    } else {
        ret = raptor_asprintf(out, "(sockaddr family=%d)", addr->sa_family);
    }
    /* This is probably redundant, but we wouldn't want to log the wrong error. */
    errno = save_errno;
    return ret;
}

void raptor_string_to_sockaddr(raptor_resolved_address* out, char* addr, int port) {
    memset(out, 0, sizeof(raptor_resolved_address));
    raptor_sockaddr_in6* addr6 = (raptor_sockaddr_in6*)out->addr;
    raptor_sockaddr_in* addr4 = (raptor_sockaddr_in*)out->addr;
    if (inet_pton(AF_INET6, addr, &addr6->sin6_addr) == 1) {
        addr6->sin6_family = AF_INET6;
        out->len = sizeof(raptor_sockaddr_in6);
    } else if (inet_pton(AF_INET, addr, &addr4->sin_addr) == 1) {
        addr4->sin_family = AF_INET;
        out->len = sizeof(raptor_sockaddr_in);
    } else {
        RAPTOR_ASSERT(0);
    }
    raptor_sockaddr_set_port(out, port);
}

int raptor_sockaddr_get_family(const raptor_resolved_address* resolved_addr) {
    const raptor_sockaddr* addr =
        reinterpret_cast<const raptor_sockaddr*>(resolved_addr->addr);
    return addr->sa_family;
}

int raptor_sockaddr_get_port(const raptor_resolved_address* resolved_addr) {
    const raptor_sockaddr* addr =
        reinterpret_cast<const raptor_sockaddr*>(resolved_addr->addr);
    switch (addr->sa_family) {
    case AF_INET:
        return ntohs(((raptor_sockaddr_in*)addr)->sin_port);
    case AF_INET6:
        return ntohs(((raptor_sockaddr_in6*)addr)->sin6_port);
    case AF_UNIX:  // is unix socket
        return 1;
    default:
        log_error("Unknown socket family %d in raptor_sockaddr_get_port", addr->sa_family);
        return 0;
    }
}

int raptor_sockaddr_set_port(
    const raptor_resolved_address* resolved_addr, int port) {
    const raptor_sockaddr* addr =
        reinterpret_cast<const raptor_sockaddr*>(resolved_addr->addr);
    if (!(port >= 0 && port < 65536)) {
        log_error("Invalid port number: %d", port);
        return 0;
    }

    switch (addr->sa_family) {
    case AF_INET:
        ((raptor_sockaddr_in*)addr)->sin_port =
            htons(static_cast<uint16_t>(port));
        return 1;
    case AF_INET6:
        ((raptor_sockaddr_in6*)addr)->sin6_port =
            htons(static_cast<uint16_t>(port));
        return 1;
    default:
        log_error("Unknown socket family %d in raptor_sockaddr_set_port", addr->sa_family);
        return 0;
    }
}
