/*
 *
 * Copyright 2017 gRPC authors.
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

#ifndef __RAPTOR_UTIL_REF_COUNTED_PTR__
#define __RAPTOR_UTIL_REF_COUNTED_PTR__

#include <algorithm>
#include <type_traits>
#include <utility>

namespace raptor {

template<typename T>
class RefCountedPtr {
public:
    RefCountedPtr(){}
    RefCountedPtr(std::nullptr_t){}

    template<typename Y>
    explicit RefCountedPtr(Y* value) {
        _value = value;
    }

    // Movable
    RefCountedPtr(RefCountedPtr&& oth) {
        _value = oth._value;
        oth._value = nullptr;
    }
    RefCountedPtr& operator= (RefCountedPtr&& oth) {
        reset(oth._value);
        oth._value = nullptr;
        return *this;
    }
    template <typename Y>
    RefCountedPtr& operator=(RefCountedPtr<Y>&& other) {
        reset(other._value);
        other._value = nullptr;
        return *this;
    }

    // Copyable
    RefCountedPtr(const RefCountedPtr& other) {
        if (other._value != nullptr) other._value->IncrementRefCount();
        _value = other._value;
    }
    template <typename Y>
    RefCountedPtr(const RefCountedPtr<Y>& other) {
        static_assert(
            std::has_virtual_destructor<T>::value,
            "T does not have a virtual dtor");
        if (other._value != nullptr) other._value->IncrementRefCount();
        _value = static_cast<T*>(other._value);
    }

    RefCountedPtr& operator=(const RefCountedPtr& other) {
        // Note: Order of reffing and unreffing is important here in case _value
        // and other._value are the same object.
        if (other._value != nullptr) other._value->IncrementRefCount();
        reset(other._value);
        return *this;
    }

    template <typename Y>
    RefCountedPtr& operator=(const RefCountedPtr<Y>& other) {
        static_assert(
        std::has_virtual_destructor<T>::value,
        "T does not have a virtual dtor");
        // Note: Order of reffing and unreffing is important here in case _value
        // and other._value are the same object.
        if (other._value != nullptr) other._value->IncrementRefCount();
        reset(other._value);
        return *this;
    }

    ~RefCountedPtr() {
        if (_value != nullptr) _value->Unref();
    }

    void swap(RefCountedPtr& other) { std::swap(_value, other._value); }

    // If value is non-null, we take ownership of a ref to it.
    void reset(T* value = nullptr) {
        if (_value != nullptr) _value->Unref();
        _value = value;
    }

    template <typename Y>
    void reset(Y* value = nullptr) {
        static_assert(
            std::has_virtual_destructor<T>::value,
            "T does not have a virtual dtor");
        if (_value != nullptr) _value->Unref();
        _value = static_cast<T*>(value);
    }

    T* release() {
        T* value = _value;
        _value = nullptr;
        return value;
    }

    T* get() const { return _value; }

    T& operator*() const { return *_value; }
    T* operator->() const { return _value; }

    template <typename Y>
    bool operator==(const RefCountedPtr<Y>& other) const {
        return _value == other._value;
    }

    template <typename Y>
    bool operator==(const Y* other) const {
        return _value == other;
    }

    bool operator==(std::nullptr_t) const { return _value == nullptr; }

    template <typename Y>
    bool operator!=(const RefCountedPtr<Y>& other) const {
        return _value != other._value;
    }

    template <typename Y>
    bool operator!=(const Y* other) const {
        return _value != other;
    }

    bool operator!=(std::nullptr_t) const { return _value != nullptr; }

private:
    template <typename Y>
    friend class RefCountedPtr;

    T* _value = nullptr;
};

template <typename T, typename... Args>
inline RefCountedPtr<T> MakeRefCounted(Args&&... args) {
    return RefCountedPtr<T>(new T(std::forward<Args>(args)...));
}
} // namespace raptor

#endif  // __RAPTOR_UTIL_REF_COUNTED_PTR__
