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

#ifndef __RAPTOR_UTIL_ALLOC__
#define __RAPTOR_UTIL_ALLOC__

#include <stddef.h>

#include <memory>
#include <type_traits>
#include <utility>

namespace raptor {

void* Malloc(size_t size);
void* ZeroAlloc(size_t size);
void* Realloc(void* ptr, size_t size);
void  Free(void* ptr);

namespace internal {

template <typename T, typename... Args>
inline T* New(Args&&... args) {
    using Type = std::aligned_storage<sizeof(T), alignof(T)>::type;
    void* ptr = Malloc(sizeof(Type));
    return new (ptr) T(std::forward<Args>(args)...);
}

template <typename T>
inline void Delete(T* ptr) {
    if (ptr != nullptr) {
        ptr->~T();
        Free(ptr);
    }
}

} // namespace internal
} // namespace raptor

#endif  // __RAPTOR_UTIL_ALLOC__