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

#include "core/resolve_address.h"
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <memory>

#include "core/host_port.h"
#include "core/sockaddr.h"

#include "util/alloc.h"
#include "util/log.h"
#include "util/string.h"
#include "util/string_view.h"
#include "util/useful.h"

raptor_error raptor_blocking_resolve_address(
                                const char* name,
                                const char* default_port,
                                raptor_resolved_addresses** addresses) {

    struct addrinfo hints;
    struct addrinfo *result = nullptr, *resp;
    int s;
    size_t i;
    raptor_error err = RAPTOR_ERROR_NONE;

    /* parse name, splitting it into host and port parts */
    raptor::UniquePtr<char> host;
    raptor::UniquePtr<char> port;
    raptor::SplitHostPort(name, &host, &port);

    if (host == nullptr) {
        err = RAPTOR_ERROR_FROM_FORMAT("unparseable host:port (%s)", name);
        goto done;
    }

    if (port == nullptr) {
        if (default_port == nullptr) {
            err = RAPTOR_ERROR_FROM_FORMAT("no port in name (%s)", name);
            goto done;
        }
        port.reset(raptor_strdup(default_port));
    }

    /* Call getaddrinfo */
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;     /* ipv4 or ipv6 */
    hints.ai_socktype = SOCK_STREAM; /* stream socket */
    hints.ai_flags = AI_PASSIVE;     /* for wildcard IP address */

    s = getaddrinfo(host.get(), port.get(), &hints, &result);
    if (s != 0) {
        /* Retry if well-known service name is recognized */
        const char* svc[][2] = {{"http", "80"}, {"https", "443"}};
        for (i = 0; i < RAPTOR_ARRAY_SIZE(svc); i++) {
            if (strcmp(port.get(), svc[i][0]) == 0) {
                s = getaddrinfo(host.get(), svc[i][1], &hints, &result);
                break;
            }
        }
    }

    if (s != 0) {
        err = RAPTOR_POSIX_ERROR("getaddrinfo");
        goto done;
    }

    /* Success path: set addrs non-NULL, fill it in */
    *addresses = static_cast<raptor_resolved_addresses*>(
        raptor::Malloc(sizeof(raptor_resolved_addresses)));
    (*addresses)->naddrs = 0;
    for (resp = result; resp != nullptr; resp = resp->ai_next) {
        (*addresses)->naddrs++;
    }
    (*addresses)->addrs = static_cast<raptor_resolved_address*>(
        raptor::Malloc(sizeof(raptor_resolved_address) * (*addresses)->naddrs));

    i = 0;
    for (resp = result; resp != nullptr; resp = resp->ai_next) {
        memcpy(&(*addresses)->addrs[i].addr, resp->ai_addr, resp->ai_addrlen);
        (*addresses)->addrs[i].len = resp->ai_addrlen;
        i++;
    }
    err = RAPTOR_ERROR_NONE;
done:
    if (result) {
        freeaddrinfo(result);
    }
    return err;
}

void raptor_resolved_addresses_destroy(raptor_resolved_addresses* addrs) {
    if (addrs != nullptr) {
        raptor::Free(addrs->addrs);
    }
    raptor::Free(addrs);
}
