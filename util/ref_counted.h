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

#ifndef __RAPTOR_UTIL_REF_COUNTED__
#define __RAPTOR_UTIL_REF_COUNTED__

#include <stdint.h>

#include "util/atomic.h"
#include "util/ref_counted_ptr.h"
#include "util/useful.h"

namespace raptor {

// PolymorphicRefCount enforces polymorphic destruction of RefCounted.
class PolymorphicRefCount {
public:
    virtual ~PolymorphicRefCount() {}
};

// NonPolymorphicRefCount does not enforce polymorphic destruction of
// RefCounted. When in doubt use PolymorphicRefCount.
class NonPolymorphicRefCount {
public:
    ~NonPolymorphicRefCount() {}
};


class RefCount {
public:
    using Value = intptr_t;
    explicit RefCount(Value init = 1)
        : _value(init) {}


    void Ref(Value n = 1) {
        _value.FetchAdd(n, MemoryOrder::RELAXED);
    }

    void RefNonZero() {
        _value.FetchAdd(1, MemoryOrder::RELAXED);
    }

    bool RefIfNonZero() {
        return _value.IncrementIfNonzero();
    }

    bool Unref() {
        const Value prior = _value.FetchSub(1, MemoryOrder::ACQ_REL);
        return prior == 1;
    }

    Value get() const {
        return _value.Load();
    }

private:
    Atomic<Value> _value;
};

template <typename Child, typename Impl = PolymorphicRefCount>
class RefCounted : public Impl {
public:
    ~RefCounted() = default;

    RefCountedPtr<Child> Ref() RAPTOR_MUST_USE_RESULT {
        IncrementRefCount();
        return RefCountedPtr<Child>(static_cast<Child*>(this));
    }

    // Make this method private, since it will only be used
    // by RefCountedPtr<>, which is a friend of this class.
    void Unref() {
        if (RAPTOR_UNLIKELY(_refs.Unref())) {
            delete static_cast<Child*>(this);
        }
    }

    bool RefIfNonZero() { return _refs.RefIfNonZero(); }

    // Not copyable nor movable.
    RefCounted(const RefCounted&) = delete;
    RefCounted& operator=(const RefCounted&) = delete;

protected:
    explicit RefCounted(intptr_t initial_refcount = 1)
        : _refs(initial_refcount) {}

 private:
    // Allow RefCountedPtr<> to access IncrementRefCount().
    template <typename T>
    friend class RefCountedPtr;

    void IncrementRefCount() { _refs.Ref(); }

    RefCount _refs;
};

} // namespace raptor

#endif  // __RAPTOR_UTIL_REF_COUNTED__
