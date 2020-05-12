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
    void (*thread_proc)(void* arg);
    void* arg;
    bool joinable;
#ifdef _WIN32
    HANDLE join_event;
#else
    pthread_t thread_id;
#endif
    char name[32];
};
}  // namespace

class InternalThreadImpl : public IThreadService {
public:
    InternalThreadImpl(
        const char* name,
        void (*thread_proc)(void* arg),
        void* arg, bool* success, const Thread::Options& options)
        : _started(false) {

#ifdef _WIN32
        _join_event = 0;
#else
        _thread_id = 0;
#endif
        RaptorMutexInit(&_mutex);
        RaptorCondVarInit(&_ready);

        ThreadImplArgs* info = static_cast<ThreadImplArgs*>(Malloc(sizeof(ThreadImplArgs)));
        info->thd = this;
        info->thread_proc = thread_proc;
        info->arg = arg;
        info->joinable = options.Joinable();
        strncpy(info->name, name, sizeof(info->name));

#ifdef _WIN32
        info->join_event = NULL;
        if (options.Joinable()) {
            info->join_event = CreateEvent(NULL, FALSE, FALSE, NULL);
            if (info->join_event == NULL) {
                Free(info);
                *success = false;
                return;
            }
        }
        HANDLE handle;
        if (options.StackSize() != 0) {
            handle = CreateThread(NULL, options.StackSize(), Run, info, 0, NULL);
        } else {
            handle = CreateThread(NULL, 64 * 1024, Run, info, 0, NULL);
        }

        if (handle == NULL) {
            CloseHandle(info->join_event);
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
        *success = pthread_create(&info->thread_id, &attr, Run, info) == 0;
        pthread_attr_destroy(&attr);
#endif

        if (!(*success)) {
            Free(info);
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
#ifdef _WIN32
        if (_join_event != NULL) {
            WaitForSingleObject(_join_event, INFINITE);
            CloseHandle(_join_event);
        }
#else
        if (_thread_id != 0) {
            pthread_join(_thread_id, nullptr);
        }
#endif
    }

private:
#ifdef _WIN32
    static DWORD WINAPI Run(void* ptr)
#else
    static void* Run(void* ptr)
#endif
    {
        ThreadImplArgs info = *(ThreadImplArgs*)ptr;
        Free(ptr);

        RaptorMutexLock(&info.thd->_mutex);
        while (!info.thd->_started) {
            RaptorCondVarWait(&info.thd->_ready, &info.thd->_mutex, -1);
        }
        RaptorMutexUnlock(&info.thd->_mutex);

        if (info.joinable) {
#ifdef _WIN32
            info.thd->_join_event = info.join_event;
#else
            info.thd->_thread_id = info.thread_id;
#endif
        } else {
            delete info.thd;
        }

        try {
            (info.thread_proc)(info.arg);
        }
        catch(...) {
            log_error("An exception occurred in %s thread", info.name);
        }
        return 0;
    }

    bool _started;
    raptor_mutex_t _mutex;
    raptor_condvar_t _ready;
#ifdef _WIN32
    HANDLE _join_event;
#else
    pthread_t _thread_id;
#endif
};

Thread::Thread()
    : _impl(nullptr)
    , _state(kNull) {
}

Thread::Thread(
    const char* thread_name,
    void (*thread_proc)(void* arg), void* arg,
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
        _impl->Join();
        delete _impl;
        _state = kFinish;
        _impl = nullptr;
    } else {
      RAPTOR_ASSERT(_state == kFailed);
    }
}
}  // namespace raptor
