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

#ifndef __RAPTOR_EXPORT_C__
#define __RAPTOR_EXPORT_C__

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

#include "raptor/export.h"
#include "raptor/types.h"

typedef struct raptor_server_t raptor_server_t;
typedef struct raptor_client_t raptor_client_t;
typedef struct raptor_protocol_t raptor_protocol_t;

// log callback
typedef void (*raptor_log_callback)(
    const char* file, int line, int level, const char* message);

RAPTOR_API int raptor_global_init();
RAPTOR_API int raptor_global_cleanup();

// ---- log ----

// log level define
#define RAPTOR_LOG_DEBUG 0
#define RAPTOR_LOG_INFO  1
#define RAPTOR_LOG_ERROR 2
#define RAPTOR_LOG_DISABLE 3

RAPTOR_API void raptor_set_log_level(int level);

// If you want to take over raptor's log output
RAPTOR_API void raptor_set_log_callback(raptor_log_callback cb);

// ---- server ----
RAPTOR_API raptor_server_t*
               raptor_server_create(const raptor_options_t* options);

RAPTOR_API void
               raptor_server_set_protocol(raptor_server_t* s, raptor_protocol_t* p);

RAPTOR_API int raptor_server_start(raptor_server_t* s);
RAPTOR_API int raptor_server_listening(raptor_server_t* s, const char* address);
RAPTOR_API int raptor_server_shutdown(raptor_server_t* s);
RAPTOR_API int raptor_server_set_callbacks(
                                raptor_server_t* s,
                                raptor_server_callback_connection_arrived on_arrived,
                                raptor_server_callback_message_received on_message_received,
                                raptor_server_callback_connection_closed on_closed
                                );

RAPTOR_API int raptor_server_send(
                                raptor_server_t* s,
                                raptor_connection_t c, const void* data, size_t len);
RAPTOR_API int raptor_server_set_userdata(
                                raptor_server_t* s, raptor_connection_t c, void* userdata);
RAPTOR_API int raptor_server_get_userdata(
                                raptor_server_t* s, raptor_connection_t c, void** userdata);
RAPTOR_API int raptor_server_set_extend_info(
                                raptor_server_t* s, raptor_connection_t c, uint64_t info);
RAPTOR_API int raptor_server_get_extend_info(
                                raptor_server_t* s, raptor_connection_t c, uint64_t* info);
RAPTOR_API int raptor_server_close_connection(
                                raptor_server_t* s, raptor_connection_t c);

RAPTOR_API void raptor_server_destroy(raptor_server_t* s);

// ---- client ----

RAPTOR_API raptor_client_t*
               raptor_client_create();

RAPTOR_API void
               raptor_client_set_protocol(raptor_client_t* c, raptor_protocol_t* p);

RAPTOR_API int raptor_client_connect(raptor_client_t* c, const char* address, size_t timeout_ms);
RAPTOR_API int raptor_client_shutdown(raptor_client_t* c);
RAPTOR_API int raptor_client_set_callbacks(
                                raptor_client_t* c,
                                raptor_client_callback_connect_result on_connect_result,
                                raptor_client_callback_message_received on_message_received,
                                raptor_client_callback_connection_closed on_closed
                                );

RAPTOR_API int raptor_client_send(
    raptor_client_t* c, const void* buff, size_t len);

RAPTOR_API void raptor_client_destroy(raptor_client_t* c);


// ---- protocol ----

RAPTOR_API raptor_protocol_t* raptor_protocol_create();
RAPTOR_API void raptor_protocol_set_callbacks(
                                raptor_protocol_t* p,
                                raptor_protocol_callback_get_max_header_size cb1,
                                raptor_protocol_callback_build_package_header cb2,
                                raptor_protocol_callback_check_package_length cb3
                                );

RAPTOR_API void raptor_protocol_destroy(raptor_protocol_t* p);

#ifdef __cplusplus
}
#endif

#endif  // __RAPTOR_EXPORT_C__
