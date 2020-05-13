/*
 *
 * Copyright 2019 gRPC authors.
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
#ifndef __RAPTOR_UTIL_STRING_VIEW__
#define __RAPTOR_UTIL_STRING_VIEW__

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <string>

#include "util/alloc.h"

namespace raptor {

// Provides a light-weight view over a char array or a slice, similar but not
// identical to absl::string_view.
//
// Any method that has the same name as absl::string_view MUST HAVE identical
// semantics to what absl::string_view provides.
//
// Methods that are not part of absl::string_view API, must be clearly
// annotated.
//
// StringView does not own the buffers that back the view. Callers must ensure
// the buffer stays around while the StringView is accessible.
//
// Pass StringView by value in functions, since it is exactly two pointers in
// size.
//
// The interface used here is not identical to absl::string_view. Notably, we
// need to support slices while we cannot support std::string, and gpr string
// style functions such as strdup() and cmp(). Once we switch to
// absl::string_view this class will inherit from absl::string_view and add the
// gRPC-specific APIs.
class StringView final {
public:
    static constexpr size_t npos = std::numeric_limits<size_t>::max();

    constexpr StringView(const char* ptr, size_t size) : _ptr(ptr), _size(size) {}
    constexpr StringView(const char* ptr)
        : StringView(ptr, ptr == nullptr ? 0 : strlen(ptr)) {}
    constexpr StringView() : StringView(nullptr, 0) {}

    constexpr const char* data() const { return _ptr; }
    constexpr size_t size() const { return _size; }
    constexpr bool empty() const { return _size == 0; }

    StringView substr(size_t start, size_t size = npos) {
        return StringView(_ptr + start, std::min(size, _size - start));
    }

    constexpr const char& operator[](size_t i) const { return _ptr[i]; }

    const char& front() const { return _ptr[0]; }
    const char& back() const { return _ptr[_size - 1]; }

    void remove_prefix(size_t n) {
        _ptr += n;
        _size -= n;
    }

    void remove_suffix(size_t n) {
        _size -= n;
    }

    size_t find(char c, size_t pos = 0) const {
        if (empty() || pos >= _size) return npos;
        const char* result =
            static_cast<const char*>(memchr(_ptr + pos, c, _size - pos));
        return result != nullptr ? result - _ptr : npos;
    }

    void clear() {
        _ptr = nullptr;
        _size = 0;
    }

    // Converts to `std::basic_string`.
    template <typename Allocator>
    explicit operator std::basic_string<char, std::char_traits<char>, Allocator>()
        const {
        if (data() == nullptr) return {};
        return std::basic_string<char, std::char_traits<char>, Allocator>(
            data(), size());
    }

private:
    const char* _ptr;
    size_t _size;
};

inline bool operator==(StringView lhs, StringView rhs) {
    return lhs.size() == rhs.size() &&
            strncmp(lhs.data(), rhs.data(), lhs.size()) == 0;
}

inline bool operator!=(StringView lhs, StringView rhs) { return !(lhs == rhs); }


struct DefaultDeleteChar {
    void operator() (char* ptr) {
        raptor::Free(ptr);
    }
};

template <typename T>
using UniquePtr = std::unique_ptr<T, DefaultDeleteChar>;

template <typename T, typename... Args>
inline std::unique_ptr<T> MakeUnique(Args&&... args) {
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

// Creates a dup of the string viewed by this class.
// Return value is null-terminated and never nullptr.
inline UniquePtr<char> StringViewToCString(const StringView sv) {
    char* str = static_cast<char*>(Malloc(sv.size() + 1));
    if (sv.size() > 0) memcpy(str, sv.data(), sv.size());
    str[sv.size()] = '\0';
    return UniquePtr<char>(str);
}

// Compares lhs and rhs.
inline int StringViewCmp(const StringView lhs, const StringView rhs) {
    const size_t len = std::min(lhs.size(), rhs.size());
    const int ret = strncmp(lhs.data(), rhs.data(), len);
    if (ret != 0) return ret;
    if (lhs.size() == rhs.size()) return 0;
    if (lhs.size() < rhs.size()) return -1;
    return 1;
}

}  // namespace raptor

#endif  // __RAPTOR_UTIL_STRING_VIEW__
