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

#ifndef __RAPTOR_EXPORT_TYPES__
#define __RAPTOR_EXPORT_TYPES__

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

typedef uint64_t ConnectionId;
typedef uint64_t raptor_connection_t;

typedef struct {
    size_t max_connections;
    size_t send_recv_timeout;
    size_t connection_timeout;
    size_t max_package_per_second;
} raptor_options_t;

typedef raptor_options_t RaptorOptions;

// server callback
typedef void (*raptor_server_callback_connection_arrived)(raptor_connection_t c);
typedef void (*raptor_server_callback_connection_closed)(raptor_connection_t c);
typedef void (*raptor_server_callback_message_received)(raptor_connection_t c, const void* buffer, size_t length);

// client callback
typedef void (*raptor_client_callback_connect_result)(int result);
typedef void (*raptor_client_callback_connection_closed)();
typedef void (*raptor_client_callback_message_received)(const void* buffer, size_t length);

// protocol callback
typedef size_t (* raptor_protocol_callback_get_max_header_size)();
typedef int    (* raptor_protocol_callback_check_package_length)(const void* buff, size_t len);

#ifdef __cplusplus
}
#endif

#endif  // __RAPTOR_EXPORT_TYPES__
