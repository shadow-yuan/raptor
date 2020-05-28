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

#ifndef __RAPTOR_CORE_SLICE_BUFFER__
#define __RAPTOR_CORE_SLICE_BUFFER__

#include <stdint.h>
#include <vector>

#include "core/slice/slice.h"

namespace raptor {

class SliceBuffer final {
public:
    SliceBuffer() : _length(0) {}
    ~SliceBuffer() = default;

    Slice Merge() const;
    size_t Count() const;
    size_t GetBufferLength() const;
    void AddSlice(const Slice& s);
    void AddSlice(Slice&& s);

    Slice GetHeader(size_t len);
    bool MoveHeader(size_t len);
    void ClearBuffer();
    bool Empty() const { return _length == 0; }
    Slice GetTopSlice() const;
    Slice GetSlice(size_t index) const;

private:
    size_t CopyToBuffer(void* buff, size_t len);

    std::vector<Slice> _vs;
    size_t _length;
};

} // namespace raptor

#endif  // __RAPTOR_CORE_SLICE_BUFFER__
