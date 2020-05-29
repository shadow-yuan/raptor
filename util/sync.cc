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

#include "util/sync.h"
#include "util/log.h"

#ifdef _WIN32
int RaptorCondVarWait(
    raptor_condvar_t* cv, raptor_mutex_t* mutex, int64_t timeout_ms) {
    int timeout = 0;
    if (timeout_ms < 0) {
        SleepConditionVariableCS(cv, mutex, INFINITE);
    } else {
        timeout = SleepConditionVariableCS(cv, mutex, static_cast<DWORD>(timeout_ms));
    }
    return timeout;
}

static void* dont_use_it = NULL;
struct raptor_init_once_parameter {
    void (*init_function)(void);
};

static BOOL CALLBACK RaptorInitOnceCallback(raptor_once_t* , void* Parameter, void** ) {
    struct raptor_init_once_parameter* p = (struct raptor_init_once_parameter*)Parameter;
    p->init_function();
    return TRUE;
}

void RaptorOnceInit(raptor_once_t* once, void (*init_function)(void)) {
    struct raptor_init_once_parameter parameter;
    parameter.init_function = init_function;
    InitOnceExecuteOnce(once, RaptorInitOnceCallback, &parameter, &dont_use_it);
}

#else
void RaptorCondVarInit(raptor_condvar_t* cv) {
    pthread_condattr_t attr;
    pthread_condattr_init(&attr);
    pthread_cond_init(cv, &attr);
}

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

void RaptorOnceInit(raptor_once_t* once_control, void (*init_routine)(void)) {
    RAPTOR_ASSERT(pthread_once(once_control, init_routine) == 0);
}

#endif
