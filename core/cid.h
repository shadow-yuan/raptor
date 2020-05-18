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

#ifndef __RAPTOR_CORE_CID__
#define __RAPTOR_CORE_CID__

#include <stdint.h>
#include "raptor/types.h"

namespace raptor {
namespace core {

constexpr ConnectionId InvalidConnectionId = (ConnectionId)(~0);

static inline ConnectionId BuildConnectionId(uint16_t magic, uint16_t listen_port, uint32_t uid) {
    uint32_t n = ((uint32_t)magic << 16) | (uint32_t)listen_port;
    uint64_t r = ((uint64_t)n << 32) | (uint64_t)uid;
    return r;
}

static inline bool VerifyConnectionId(ConnectionId cid, uint16_t magic) {
    if (cid == InvalidConnectionId) return false;
    uint32_t n = (uint32_t)(cid >> 32);
    uint16_t r = (uint16_t)(n >> 16);
    return (magic == r);
}

static inline uint16_t GetMagicNumber(ConnectionId cid) {
    uint32_t n = (uint32_t)(cid >> 32);
    return (uint16_t)(n >> 16);
}

static inline uint16_t GetListenPort(ConnectionId cid) {
    uint32_t n = (uint32_t)(cid >> 32);
    return (uint16_t)(n & 0xffff);;
}

static inline uint32_t GetUserId(ConnectionId cid) {
    return (uint32_t)(cid & 0xffffffff);
}

}  // namespace core
}  // namespace raptor
#endif  // __RAPTOR_CORE_CID__
