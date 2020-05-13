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

#ifndef __RAPTOR_CORE_SOCKET_UTIL__
#define __RAPTOR_CORE_SOCKET_UTIL__

#include "core/resolve_address.h"

/* Returns true if addr is an IPv4-mapped IPv6 address within the
   ::ffff:0.0.0.0/96 range, or false otherwise.

   If addr4_out is non-NULL, the inner IPv4 address will be copied here when
   returning true. */
int raptor_sockaddr_is_v4mapped(const raptor_resolved_address* resolved_addr,
                                raptor_resolved_address* resolved_addr4_out);

/* If addr is an AF_INET address, writes the corresponding ::ffff:0.0.0.0/96
   address to addr6_out and returns true.  Otherwise returns false. */
int raptor_sockaddr_to_v4mapped(const raptor_resolved_address* resolved_addr,
                                raptor_resolved_address* resolved_addr6_out);

/* If addr is ::, 0.0.0.0, or ::ffff:0.0.0.0, writes the port number to
 *port_out (if not NULL) and returns true, otherwise returns false. */
int raptor_sockaddr_is_wildcard(const raptor_resolved_address* resolved_addr,
                                int* port_out);

/* Writes 0.0.0.0:port and [::]:port to separate sockaddrs. */
void grpc_sockaddr_make_wildcards(int port, raptor_resolved_address* wild4_out,
                                  raptor_resolved_address* wild6_out);

/* Writes 0.0.0.0:port. */
void raptor_sockaddr_make_wildcard4(int port,
                                    raptor_resolved_address* resolved_wild_out);

/* Writes [::]:port. */
void raptor_sockaddr_make_wildcard6(int port,
                                    raptor_resolved_address* resolved_wild_out);

/* Converts a sockaddr into a newly-allocated human-readable string.

   Currently, only the AF_INET and AF_INET6 families are recognized.
   If the normalize flag is enabled, ::ffff:0.0.0.0/96 IPv6 addresses are
   displayed as plain IPv4.

   Usage is similar to gpr_asprintf: returns the number of bytes written
   (excluding the final '\0'), and *out points to a string which must later be
   destroyed using gpr_free().

   In the unlikely event of an error, returns -1 and sets *out to NULL.
   The existing value of errno is always preserved. */
int raptor_sockaddr_to_string(char** out,
                              const raptor_resolved_address* resolved_addr,
                              int normalize);


void raptor_string_to_sockaddr(
    raptor_resolved_address* out, char* addr, int port);

int raptor_sockaddr_get_family(
    const raptor_resolved_address* resolved_addr);

/* Set IP port number of a sockaddr */
int raptor_sockaddr_set_port(
    const raptor_resolved_address* resolved_addr, int port);

/* Return the IP port number of a sockaddr */
int raptor_sockaddr_get_port(
    const raptor_resolved_address* resolved_addr);

#endif  // __RAPTOR_CORE_SOCKET_UTIL__
