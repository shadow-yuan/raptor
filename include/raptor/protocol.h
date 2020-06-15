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

#ifndef __RAPTOR_PROTOCOL__
#define __RAPTOR_PROTOCOL__

#include <stddef.h>
#include "raptor/types.h"

namespace raptor {
class IProtocol {
public:
    virtual ~IProtocol() {}

    // Get the max header size of current protocol
    virtual size_t GetMaxHeaderSize() = 0;

    // return -1: error;  0: need more data; > 0 : pack_len
    virtual int CheckPackageLength(ConnectionId cid, const void* data, size_t len) = 0;
};
} // namespace raptor
#endif  // __RAPTOR_PROTOCOL__
