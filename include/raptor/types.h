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

#include <stdint.h>

typedef uint64_t ConnectionId;
typedef uint64_t raptor_connection_t;

typedef struct {
    int max_connections;
    int send_recv_timeout;
    int send_recv_threads;
    int accept_threads;
} raptor_options_t;

typedef raptor_options_t RaptorOptions;

#ifdef __cplusplus
}
#endif

#endif  // __RAPTOR_EXPORT_TYPES__
