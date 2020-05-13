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

#ifndef __RAPTOR_CORE_RESOLVE_ADDRESS__
#define __RAPTOR_CORE_RESOLVE_ADDRESS__

#include <stddef.h>
#include <stdint.h>

#include "util/status.h"

typedef struct {
    char addr[128];
    uint32_t len;
} raptor_resolved_address;

typedef struct {
    size_t naddrs;
    raptor_resolved_address* addrs;
} raptor_resolved_addresses;

/* Resolve addr in a blocking fashion. On success,
   default_port can be nullptr, or "https" or "http"
   result must be freed with grpc_resolved_addresses_destroy. */
raptor_error raptor_blocking_resolve_address(
                    const char* name,
                    const char* default_port,
                    raptor_resolved_addresses** addresses);


void raptor_resolved_addresses_destroy(raptor_resolved_addresses* addrs);

#endif  // __RAPTOR_CORE_RESOLVE_ADDRESS__
