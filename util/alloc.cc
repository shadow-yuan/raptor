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

#include "util/alloc.h"
#include <stdlib.h>

namespace raptor {

void* Malloc(size_t size) {
    if (size > 0) {
        void* ptr = ::malloc(size);
        if (!ptr) {
            abort();
        }
        return ptr;
    }
    return nullptr;
}

void* ZeroAlloc(size_t size) {
    if (size > 0) {
        void* p = ::calloc(size, 1);
        if (!p) {
            abort();
        }
        return p;
    }
    return nullptr;
}

void* Realloc(void* ptr, size_t size) {
    if ((size == 0) && (ptr == nullptr)) {
        return nullptr;
    }
    ptr = ::realloc(ptr, size);
    if (!ptr) {
        abort();
    }
    return ptr;
}

void Free(void* ptr) {
    if (ptr) free(ptr);
}

} // namespace raptor
