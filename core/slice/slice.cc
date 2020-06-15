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

#include "core/slice/slice.h"
#include <string.h>
#include <algorithm>
#include "util/alloc.h"
#include "util/atomic.h"

namespace raptor {
class SliceRefCount final {
public:
    SliceRefCount();
    ~SliceRefCount() {}

    SliceRefCount(const SliceRefCount&) = delete;
    SliceRefCount& operator= (const SliceRefCount&) = delete;

    void AddRef();
    void DecRef();

private:
    AtomicInt32 _refs;
};

SliceRefCount::SliceRefCount() {
    _refs.Store(1, MemoryOrder::RELEASE);
}

void SliceRefCount::AddRef() {
    _refs.FetchAdd(1, MemoryOrder::RELAXED);
}

void SliceRefCount::DecRef() {
    int32_t n = _refs.FetchSub(1, MemoryOrder::ACQ_REL);
    if (n == 1) {
        Free(this);
    }
}

// ------------------------------------------

Slice::Slice(const char* ptr) : Slice(ptr, strlen(ptr)) {}
Slice::Slice(const void* buf, size_t len) : Slice((const char*)buf, len) {}
Slice::Slice(void* buf, size_t len) : Slice((const char*)buf, len) {}

Slice::Slice(const char* ptr, size_t len) {
    if (len == 0) {  // empty object
        _refs = nullptr;
        memset(&_data, 0, sizeof(_data));
        return;
    }

    if (len <= SLICE_INLINED_SIZE) {
        _refs = nullptr;
        _data.inlined.length = static_cast<uint8_t>(len);
        if (ptr) {
            memcpy(_data.inlined.bytes, ptr, len);
        }
    } else {
        /*  Memory layout used by the slice created here:

            +-----------+----------------------------------------------------------+
            | refcount  | bytes                                                    |
            +-----------+----------------------------------------------------------+

            refcount is a SliceRefCount
            bytes is an array of bytes of the requested length
        */

        _refs = (SliceRefCount*)Malloc(sizeof(SliceRefCount) + len);
        new (_refs) SliceRefCount;
        _data.refcounted.length = len;
        _data.refcounted.bytes = reinterpret_cast<uint8_t*>(_refs + 1);
        if (ptr) {
            memcpy(_data.refcounted.bytes, ptr, len);
        }
    }
}

Slice::Slice() {
    _refs = nullptr;
    memset(&_data, 0, sizeof(_data));
}

Slice::~Slice() {
    if (_refs) {
        _refs->DecRef();
    }
}

Slice::Slice(const Slice& oth) {
    if(oth._refs != nullptr) {
        oth._refs->AddRef();
    }
    _refs = oth._refs;
    _data = oth._data;
}

Slice& Slice::operator= (const Slice& oth) {
    if (this != &oth) {
        if (_refs) {
            _refs->DecRef();
        }
        if (oth._refs) {
            oth._refs->AddRef();
        }
        _refs = oth._refs;
        _data = oth._data;
    }
    return *this;
}

Slice::Slice(Slice&& oth) {
    if (oth._refs) {
        oth._refs->AddRef();
    }
    _refs = std::move(oth._refs);
    _data = std::move(oth._data);
}

Slice& Slice::operator= (Slice&& oth) {
    if (this != &oth) {
        if (oth._refs) {
            oth._refs->AddRef();
        }
        if (_refs) {
            _refs->DecRef();
        }
        _refs = std::move(oth._refs);
        _data = std::move(oth._data);
    }
    return *this;
}

size_t Slice::size() const {
    return
        (_refs)
         ? _data.refcounted.length
         : _data.inlined.length;
}

const uint8_t* Slice::begin() const {
    return
        (_refs)
         ? _data.refcounted.bytes
         : _data.inlined.bytes;
}

const uint8_t* Slice::end() const {
    return begin() + size();
}

void Slice::CutTail(size_t cut_size) {
    if (cut_size == 0) return;
    if (cut_size > size()) {
        cut_size = size();
    }
    if (_refs) {
        _data.refcounted.length -= cut_size;
    } else {
        _data.inlined.length -= static_cast<uint8_t>(cut_size);
    }
}

// -----------------------------

Slice MakeSliceByDefaultSize() {
    size_t len = 4096 - sizeof(SliceRefCount);
    return MakeSliceByLength(len);
}

Slice MakeSliceByLength(size_t len) {
    Slice s;
    if (len <= Slice::SLICE_INLINED_SIZE) {
        s._refs = nullptr;
        s._data.inlined.length = static_cast<uint8_t>(len);
    } else {
        s._refs = (SliceRefCount*)Malloc(sizeof(SliceRefCount) + len);
        new (s._refs) SliceRefCount;
        s._data.refcounted.length = len;
        s._data.refcounted.bytes = reinterpret_cast<uint8_t*>(s._refs + 1);
    }
    return s;
}

Slice operator+ (Slice s1, Slice s2) {
    if (s1.Empty() && s2.Empty()) {
        return Slice();
    }
    size_t len = s1.size() + s2.size();
    Slice s = MakeSliceByLength(len);
    uint8_t* buff = s.Buffer();
    if (!s1.Empty()) {
        memcpy(buff, s1.Buffer(), s1.size());
        buff += s1.size();
    }
    if (!s2.Empty()) {
        memcpy(buff, s2.Buffer(), s2.size());
    }
    return s;
}

Slice operator- (Slice s1, size_t length) {
    if (s1.size() <= length ) {
        return Slice();
    }

    size_t len = s1.size() - length;
    Slice s;
    if (len <= Slice::SLICE_INLINED_SIZE) {
        s._refs = nullptr;
        s._data.inlined.length = static_cast<uint8_t>(len);
        memcpy(s._data.inlined.bytes, s1.begin() + length, len);
    } else {
        s = s1;
        s._data.refcounted.length = len;
        s._data.refcounted.bytes = s1._data.refcounted.bytes + length;
    }
    return s;
}
} // namespace raptor
