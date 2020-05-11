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
#define RAPTOR_MUTEX_INIT(m) \
    InitializeCriticalSection(m)

#define RAPTOR_MUTEX_LOCK(m) \
    EnterCriticalSection(m)

#define RAPTOR_MUTEX_UNLOCK(m) \
    LeaveCriticalSection(m)

#define RAPTOR_MUTEX_DESTROY(m) \
    DeleteCriticalSection(m)

// condvar

#define RAPTOR_CONDVAR_INIT(cv) \
    InitializeConditionVariable(cv)

#define RAPTOR_CONDVAR_DESTROY(cv) (cv)

// Return 1 in timeout, 0 in other cases
static inline
int RAPTOR_CONDVAR_WAIT(
    raptor_condvar_t* cv, raptor_mutex_t* mutex, int64_t timeout_ms) {
    int timeout = 0;
    if (timeout_ms < 0) {
        SleepConditionVariableCS(cv, mutex, INFINITE);
    } else {
        timeout = SleepConditionVariableCS(cv, mutex, timeout_ms);
    }
    return timeout;
}

#define RAPTOR_CONDVAR_SIGNAL(cv) \
    WakeConditionVariable(cv)

#define RAPTOR_CONDVAR_BROADCAST(cv) \
    WakeAllConditionVariable(cv)

#else
#include <errno.h>
#include <pthread.h>
#include <sys/time.h>

typedef pthread_mutex_t raptor_mutex_t;
typedef pthread_cond_t  raptor_condvar_t;

#define RAPTOR_MUTEX_INIT(m) \
    pthread_mutex_init(m, NULL)

#define RAPTOR_MUTEX_LOCK(m) \
    pthread_mutex_lock(m)

#define RAPTOR_MUTEX_UNLOCK(m) \
    pthread_mutex_unlock(m)

#define RAPTOR_MUTEX_DESTROY(m) \
    pthread_mutex_destroy(m)

// condvar

#define RAPTOR_CONDVAR_INIT(cv)        \
    {                                  \
        pthread_condattr_t attr_;      \
        pthread_condattr_init(&attr_); \
        pthread_cond_init(cv, &attr_); \
    }

#define RAPTOR_CONDVAR_DESTROY(cv) \
    pthread_cond_destroy(cv)

// Return 1 in timeout, 0 in other cases
static inline
int RAPTOR_CONDVAR_WAIT(
    raptor_condvar_t* cv, raptor_mutex_t* mutex, int64_t timeout_ms) {
    int error = 0;
    if (timeout_ms < 0) {
        error = pthread_cond_wait(cv, mutex);
    } else {
        struct timeval now;
        gettimeofday(&now, nullptr);
        now.tv_sec += (timeout_ms / 1000);
        now.tv_usec += (timeout_ms % 1000) * 1000;
        struct timespec abstime;
        abstime.tv_sec = now.tv_sec;
        abstime.tv_nsec = now.tv_usec * 1000;
        error = pthread_cond_timedwait(cv, mutex, timeout_ms);
    }
    return (err == ETIMEDOUT) ? 1 : 0;
}

#define RAPTOR_CONDVAR_SIGNAL(cv) \
    pthread_cond_signal(cv)

#define RAPTOR_CONDVAR_BROADCAST(cv) \
    pthread_cond_broadcast(cv)

#endif

#endif  // __RAPTOR_UTIL_MUTEX_CONDVAR__
