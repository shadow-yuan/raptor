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

#ifndef __RAPTOR_UTIL_THREAD__
#define __RAPTOR_UTIL_THREAD__

#include <stddef.h>
#include <functional>

namespace raptor {

class IThreadService {
public:
    virtual ~IThreadService() {}
    virtual void Start() = 0;
    virtual void Join() = 0;
};

/*
    Thread executor, allows to bring a custom parameter,
    which can be used to distinguish different threads,
    or just set nullptr.
*/
using ThreadExecutor = std::function<void(void*)>;

class Thread {
public:
    class Options {
    public:
        Options() : _joinable(true), _stack_size(0) {}

        Options& SetJoinable(bool joinable) {
            _joinable = joinable;
            return *this;
        }

        bool Joinable() const { return _joinable; }

        Options& SetStackSize(size_t size) {
            _stack_size = size;
            return *this;
        }

        size_t StackSize() const { return _stack_size; }

    private:
        bool _joinable;
        size_t _stack_size;
    }; // class Options

    Thread();
    Thread(
        const char* thread_name,
        ThreadExecutor, void* arg,
        bool* success = nullptr, const Options& options = Options());

    ~Thread() = default;

    Thread(const Thread&) = delete;
    Thread& operator= (const Thread&) = delete;

    // movable construction
    Thread(Thread && oth);
    Thread& operator= (Thread && oth);

    void Start();
    void Join();

private:

    enum State {
        kNull,
        kAlive,
        kActive,
        kFinish,
        kFailed,
    };

    IThreadService* _impl;
    State _state;
};

}  // namespace raptor

#endif  // __RAPTOR_UTIL_THREAD__
