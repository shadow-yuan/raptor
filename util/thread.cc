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

#include "util/thread.h"
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <unistd.h>
#endif
#include "util/alloc.h"
#include "util/log.h"
#include "util/sync.h"
#include "util/useful.h"

namespace raptor {
class InternalThreadImpl;
namespace {
struct ThreadImplArgs {
    InternalThreadImpl* thd;
    ThreadExecutor thread_proc;
    void* arg;
    bool joinable;
#ifdef _WIN32
    HANDLE join_object;
#else
    pthread_t join_object;
#endif
    char name[32];
};
}  // namespace

class InternalThreadImpl : public IThreadService {
public:
    InternalThreadImpl(
        const char* name,
        ThreadExecutor thread_proc,
        void* arg, bool* success, const Thread::Options& options)
        : _started(false) {

        _join_object = 0;
        RaptorMutexInit(&_mutex);
        RaptorCondVarInit(&_ready);

        ThreadImplArgs* info = new ThreadImplArgs;
        info->thd = this;
        info->thread_proc = thread_proc;
        info->arg = arg;
        info->joinable = options.Joinable();
        strncpy(info->name, name, sizeof(info->name));

#ifdef _WIN32
        info->join_object = NULL;
        if (options.Joinable()) {
            info->join_object = CreateEvent(NULL, FALSE, FALSE, NULL);
            if (info->join_object == NULL) {
                delete info;
                *success = false;
                return;
            }
        }

        size_t dwStackSize = (options.StackSize() == 0) ? (64 * 1024) : options.StackSize();
        HANDLE handle = CreateThread(NULL, dwStackSize, Run, info, 0, NULL);
        if (handle == NULL) {
            CloseHandle(info->join_object);
            *success = false;
        } else {
            CloseHandle(handle);
            *success = true;
        }
#else
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr,
            options.Joinable() ? PTHREAD_CREATE_JOINABLE : PTHREAD_CREATE_DETACHED);
        if (options.StackSize() != 0) {
            pthread_attr_setstacksize(&attr, options.StackSize());
        }
        *success = pthread_create(&info->join_object, &attr, Run, info) == 0;
        pthread_attr_destroy(&attr);
#endif

        if (*success) {
            _join_object = info->join_object;
        } else {
            delete info;
        }
    }

    ~InternalThreadImpl() {
        RaptorMutexDestroy(&_mutex);
        RaptorCondVarDestroy(&_ready);
    }

    void Start() {
        RaptorMutexLock(&_mutex);
        _started = true;
        RaptorCondVarSignal(&_ready);
        RaptorMutexUnlock(&_mutex);
    }

    void Join() {
        if (_join_object != 0) {
#ifdef _WIN32
            WaitForSingleObject(_join_object, INFINITE);
            CloseHandle(_join_object);
#else
            pthread_join(_join_object, nullptr);
#endif
        }
    }

private:
#ifdef _WIN32
    static DWORD WINAPI Run(void* ptr) {
#else
    static void* Run(void* ptr) {
#endif
        ThreadImplArgs info = *(ThreadImplArgs*)ptr;
        delete (ThreadImplArgs*)ptr;

        RaptorMutexLock(&info.thd->_mutex);
        while (!info.thd->_started) {
            RaptorCondVarWait(&info.thd->_ready, &info.thd->_mutex, -1);
        }
        RaptorMutexUnlock(&info.thd->_mutex);

        try {
            (info.thread_proc)(info.arg);
        } catch(...) {
            log_error("An exception occurred in %s thread", info.name);
        }

#ifdef _WIN32
        if (info.joinable) {
            SetEvent(info.join_object);
        }
#endif
        if (!info.joinable) {
            delete info.thd;
        }
        return 0;
    }

private:
    bool _started;
    raptor_mutex_t _mutex;
    raptor_condvar_t _ready;
#ifdef _WIN32
    HANDLE _join_object;
#else
    pthread_t _join_object;
#endif
};

Thread::Thread()
    : _impl(nullptr)
    , _state(kNull) {
}

Thread::Thread(
    const char* thread_name,
    ThreadExecutor thread_proc, void* arg,
    bool* success, const Options& options) {
    bool ret = false;
    _impl = new InternalThreadImpl(thread_name, thread_proc, arg, &ret, options);
    if (!ret) {
        _state = kFailed;
        delete _impl;
        _impl = nullptr;
    } else {
        _state = kAlive;
    }
    if (success) {
        *success = ret;
    }
}

Thread::Thread(Thread && other)
    : _impl(other._impl), _state(other._state) {
    other._impl = nullptr;
    other._state = kNull;
}

Thread& Thread::operator= (Thread && other) {
    if (this != &other) {
        _impl = other._impl;
        _state = other._state;
        other._impl = nullptr;
        other._state = kNull;
    }
    return *this;
}

void Thread::Start() {
    if (_impl != nullptr) {
        RAPTOR_ASSERT(_state == kAlive);
        _state = kActive;
        _impl->Start();
    } else {
        RAPTOR_ASSERT(_state == kFailed);
    }
}

void Thread::Join() {
    if (_impl != nullptr) {
        if (_state == kAlive) {
            _state = kActive;
            _impl->Start();
        }
        _impl->Join();
        delete _impl;
        _state = kFinish;
        _impl = nullptr;
    } else {
        RAPTOR_ASSERT(_state == kFailed);
    }
}
}  // namespace raptor
