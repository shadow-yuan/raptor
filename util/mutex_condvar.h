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

#ifndef __RAPTOR_UTIL_MUTEX_CONDVAR__
#define __RAPTOR_UTIL_MUTEX_CONDVAR__

#include <stdint.h>

#ifdef _WIN32
#include <windows.h>

typedef CRITICAL_SECTION raptor_mutex_t;
typedef CONDITION_VARIABLE raptor_condvar_t;

// mutex
#define RaptorMutexInit(m) \
    InitializeCriticalSection(m)

#define RaptorMutexLock(m) \
    EnterCriticalSection(m)

#define RaptorMutexUnlock(m) \
    LeaveCriticalSection(m)

#define RaptorMutexDestroy(m) \
    DeleteCriticalSection(m)

// condvar

#define RaptorCondVarInit(cv) \
    InitializeConditionVariable(cv)

#define RaptorCondVarDestroy(cv) (cv)

// Return 1 in timeout, 0 in other cases
static inline
int RaptorCondVarWait(
    raptor_condvar_t* cv, raptor_mutex_t* mutex, int64_t timeout_ms) {
    int timeout = 0;
    if (timeout_ms < 0) {
        SleepConditionVariableCS(cv, mutex, INFINITE);
    } else {
        timeout = SleepConditionVariableCS(cv, mutex, timeout_ms);
    }
    return timeout;
}

#define RaptorCondVarSignal(cv) \
    WakeConditionVariable(cv)

#define RaptorCondVarBroadcast(cv) \
    WakeAllConditionVariable(cv)

#else
#include <errno.h>
#include <pthread.h>
#include <sys/time.h>

typedef pthread_mutex_t raptor_mutex_t;
typedef pthread_cond_t  raptor_condvar_t;

#define RaptorMutexInit(m) \
    pthread_mutex_init(m, NULL)

#define RaptorMutexLock(m) \
    pthread_mutex_lock(m)

#define RaptorMutexUnlock(m) \
    pthread_mutex_unlock(m)

#define RaptorMutexDestroy(m) \
    pthread_mutex_destroy(m)

// condvar

static inline
void RaptorCondVarInit(raptor_condvar_t* cv) {
    pthread_condattr_t attr;
    pthread_condattr_init(&attr);
    pthread_cond_init(cv, &attr);
}

#define RaptorCondVarDestroy(cv) \
    pthread_cond_destroy(cv)

// Return 1 in timeout, 0 in other cases
static inline
int RaptorCondVarWait(
    raptor_condvar_t* cv, raptor_mutex_t* mutex, int64_t timeout_ms) {
    int error = 0;
    if (timeout_ms < 0) {
        error = pthread_cond_wait(cv, mutex);
    } else {
        struct timeval now;
        gettimeofday(&now, nullptr);
        now.tv_sec += static_cast<time_t>(timeout_ms / 1000);
        now.tv_usec += static_cast<long>((timeout_ms % 1000) * 1000);
        long sec = now.tv_usec / 1000000;
        if (sec > 0) {
            now.tv_sec += sec;
            now.tv_usec -= sec * 1000000;
        }
        struct timespec abstime;
        abstime.tv_sec = now.tv_sec;
        abstime.tv_nsec = now.tv_usec * 1000;
        error = pthread_cond_timedwait(cv, mutex, &abstime);
    }
    return (error == ETIMEDOUT) ? 1 : 0;
}

#define RaptorCondVarSignal(cv) \
    pthread_cond_signal(cv)

#define RaptorCondVarBroadcast(cv) \
    pthread_cond_broadcast(cv)

#endif

#endif  // __RAPTOR_UTIL_MUTEX_CONDVAR__
