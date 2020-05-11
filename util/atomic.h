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

#ifndef __RAPTOR_UTIL_ATOMIC__
#define __RAPTOR_UTIL_ATOMIC__

#include <stdint.h>
#include <atomic>

namespace raptor {
enum class MemoryOrder {
    RELAXED = std::memory_order_relaxed,
    CONSUME = std::memory_order_consume,
    ACQUIRE = std::memory_order_acquire,
    RELEASE = std::memory_order_release,
    ACQ_REL = std::memory_order_acq_rel,
    SEQ_CST = std::memory_order_seq_cst
};

template <typename T>
class Atomic {
public:
    explicit Atomic(T val = T()) : _value(val) {}

    T Load(MemoryOrder order = MemoryOrder::RELAXED) const {
        return _value.load(static_cast<std::memory_order>(order));
    }

    void Store(T val, MemoryOrder order = MemoryOrder::RELAXED) {
        _value.store(val, static_cast<std::memory_order>(order));
    }

    T Exchange(T desired, MemoryOrder order) {
        return _value.exchange(desired, static_cast<std::memory_order>(order));
    }

    bool CompareExchangeWeak(T* expected, T desired, MemoryOrder success,
                            MemoryOrder failure) {
        return _value.compare_exchange_weak(
            *expected, desired, static_cast<std::memory_order>(success),
            static_cast<std::memory_order>(failure));
    }

    bool CompareExchangeStrong(T* expected, T desired, MemoryOrder success,
                                MemoryOrder failure) {
        return _value.compare_exchange_strong(
            *expected, desired, static_cast<std::memory_order>(success),
            static_cast<std::memory_order>(failure));
    }

    template <typename Arg>
    T FetchAdd(Arg arg, MemoryOrder order = MemoryOrder::SEQ_CST) {
        return _value.fetch_add(
            static_cast<Arg>(arg), static_cast<std::memory_order>(order));
    }

    template <typename Arg>
    T FetchSub(Arg arg, MemoryOrder order = MemoryOrder::SEQ_CST) {
        return _value.fetch_sub(
            static_cast<Arg>(arg), static_cast<std::memory_order>(order));
    }

    bool IncrementIfNonzero(MemoryOrder load_order = MemoryOrder::ACQUIRE) {
        T count = _value.load(static_cast<std::memory_order>(load_order));
        do {
            if (count == 0) {
                return false;
            }
        } while (!CompareExchangeWeak(
            &count, count + 1, MemoryOrder::ACQ_REL, load_order));
        return true;
    }

private:
    std::atomic<T> _value;
};

using AtomicBool   = Atomic<bool>;
using AtomicInt32  = Atomic<int32_t>;
using AtomicInt64  = Atomic<int64_t>;
using AtomicUInt32 = Atomic<uint32_t>;
using AtomicUInt64 = Atomic<uint64_t>;
using AtomicIntptr = Atomic<intptr_t>;

}  // namespace raptor

#endif  // __RAPTOR_UTIL_ATOMIC__
