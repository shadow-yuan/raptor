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

#ifndef __RAPTOR_UTIL_SYNC__
#define __RAPTOR_UTIL_SYNC__

#include "util/mutex_condvar.h"

namespace raptor {
class Mutex final {
public:
    Mutex() { RaptorMutexInit(&_mutex); }
    ~Mutex() { RaptorMutexDestroy(&_mutex); }

    Mutex(const Mutex&) = delete;
    Mutex& operator=(const Mutex&) = delete;

    void Lock() { RaptorMutexLock(&_mutex); }
    void Unlock() { RaptorMutexUnlock(&_mutex); }

    operator raptor_mutex_t* () { return &_mutex; }

private:
    raptor_mutex_t _mutex;
};

class AutoMutex final {
public:
    explicit AutoMutex(Mutex* m) : _mutex(*m) {
        RaptorMutexLock(_mutex);
    }
    explicit AutoMutex(raptor_mutex_t* m) : _mutex(m) {
        RaptorMutexLock(_mutex);
    }
    ~AutoMutex() { RaptorMutexUnlock(_mutex); }

    AutoMutex(const AutoMutex&) = delete;
    AutoMutex& operator=(const AutoMutex&) = delete;

private:
    raptor_mutex_t* _mutex;
};

class ConditionVariable final {
public:
    ConditionVariable() { RaptorCondVarInit(&_cv); }
    ~ConditionVariable() { RaptorCondVarDestroy(&_cv); }

    ConditionVariable(const ConditionVariable&) = delete;
    ConditionVariable& operator=(const ConditionVariable&) = delete;

    void Signal() { RaptorCondVarSignal(&_cv); }
    void Broadcast() { RaptorCondVarBroadcast(&_cv); }

    int Wait(Mutex* m) {
        return RaptorCondVarWait(&_cv, *m, -1);
    }
    int Wait(Mutex* m, int64_t timeout_ms) {
        return RaptorCondVarWait(&_cv, *m, timeout_ms);
    }

    operator raptor_condvar_t* () { return &_cv; }

private:
    raptor_condvar_t _cv;
};

} // namespace raptor


#endif  // __RAPTOR_UTIL_SYNC__
