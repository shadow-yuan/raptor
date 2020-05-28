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

#include "core/slice/slice_buffer.h"
#include <string.h>
#include "util/alloc.h"
#include "util/log.h"
#include "util/useful.h"

namespace raptor {

Slice SliceBuffer::Merge() const {

    if (Count() == 0) {
        return Slice();
    }

    if (Count() == 1) {
        return _vs[0];
    }

    size_t len = GetBufferLength();
    Slice ret = MakeSliceByLength(len);
    uint8_t* buf = ret.Buffer();
    for (auto it = _vs.begin(); it != _vs.end(); ++it) {
        size_t blk_size = it->size();
        if (blk_size > 0) {
            memcpy(buf, it->begin(), blk_size);
            buf += blk_size;
        }
    }
    return ret;
}

size_t SliceBuffer::Count() const {
    return _vs.size();
}

size_t SliceBuffer::GetBufferLength() const {
    return _length;
}

void SliceBuffer::AddSlice(const Slice& s) {
    _vs.push_back(s);
    _length += s.size();
}

void SliceBuffer::AddSlice(Slice&& s) {
    _vs.push_back(s);
    _length += s.size();
}

Slice SliceBuffer::GetHeader(size_t len) {
    if(len == 0 || GetBufferLength() < len) {
        return Slice();
    }

    Slice s = MakeSliceByLength(len);
    CopyToBuffer(s.Buffer(), len);
    return s;
}

bool SliceBuffer::MoveHeader(size_t len) {
    if(GetBufferLength() < len) {
        return false;
    }
    if(len == 0) {
        return true;
    }

    auto it = _vs.begin();
    size_t left = it->size();

    if(left > len) {
        *it = *it - len;
        _length -= len;
    } else if(left == len) {
        _length -= len;
        _vs.erase(it);
    } else {
        // len > left
        _length -= left;
        _vs.erase(it);

        return MoveHeader(len - left);
    }
    return true;
}

size_t SliceBuffer::CopyToBuffer(void* buffer, size_t length) {
    RAPTOR_ASSERT(length <= GetBufferLength());

    auto it = _vs.begin();

    size_t left = length;
    size_t pos = 0;

    while(it != _vs.end() && left != 0) {

        size_t len = RAPTOR_MIN(left, it->size());
        memcpy((uint8_t*)buffer + pos, it->begin(), len);

        left -= len;
        pos += len;

        ++it;
    }

    return pos;
}

void SliceBuffer::ClearBuffer() {
    _vs.clear();
    _length = 0;
}

Slice SliceBuffer::GetTopSlice() const {
    if (_vs.empty()) {
        return Slice();
    }
    return _vs[0];
}

Slice SliceBuffer::GetSlice(size_t index) const {
    if (index < _vs.size()) {
        return _vs[index];
    }
    return Slice();
}

} // namespace raptor
