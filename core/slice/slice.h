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

#ifndef __RAPTOR_EXPORT_SLICE__
#define __RAPTOR_EXPORT_SLICE__

#include <stddef.h>
#include <stdint.h>

namespace raptor {
class SliceRefCount;

class Slice final {
public:
    Slice();
    Slice(const char* ptr);
    Slice(const void* buf, size_t len);
    Slice(void* buf, size_t len);
    Slice(const char* ptr, size_t len);

    ~Slice();

    Slice(const Slice& oth);
    Slice& operator= (const Slice& oth);

    Slice(Slice&& oth);
    Slice& operator= (Slice&& oth);

    size_t size() const;
    const uint8_t* begin() const;
    const uint8_t* end() const;

    inline size_t Length() const { return size(); }
    inline bool Empty() const { return (size() == 0); }
    inline uint8_t* Buffer() {
        return const_cast<uint8_t*>(begin());
    }
    void CutTail(size_t cut_size);

private:

    enum : int { SLICE_INLINED_SIZE = 23 };

    SliceRefCount* _refs;
    union slice_data {
        struct {
            size_t length;
            uint8_t* bytes;
        } refcounted;
        struct {
            uint8_t length;
            uint8_t bytes[SLICE_INLINED_SIZE];
        } inlined;
    } _data;

    friend class SliceBuffer;
    friend Slice MakeSliceByDefaultSize();
    friend Slice MakeSliceByLength(size_t len);
    friend Slice operator+ (Slice s1, Slice s2);
    friend Slice operator- (Slice s1, size_t len);
};

// The default length is less than 4096
Slice MakeSliceByDefaultSize();

Slice MakeSliceByLength(size_t len);

// Combine the data of s1 and s2,
// s1 is in the front, s2 is in the back
Slice operator+ (Slice s1, Slice s2);

// Remove len bytes from the begin address
Slice operator- (Slice s1, size_t len);

} // namespace raptor

#endif  // __RAPTOR_EXPORT_SLICE__
