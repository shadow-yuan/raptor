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

#include "util/cpu.h"
#include "util/sync.h"
#ifdef _WIN32
#include <sysinfoapi.h>
#else
#include <unistd.h>
#endif

static int number_of_cpu_cores = 0;

#ifdef _WIN32

static void get_number_of_cpu_cores_impl() {
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    number_of_cpu_cores = (int)si.dwNumberOfProcessors;
    if (number_of_cpu_cores < 1) {
        number_of_cpu_cores = 1;
    }
}

#else

static void get_number_of_cpu_cores_impl() {
    number_of_cpu_cores = static_cast<int>(sysconf(_SC_NPROCESSORS_CONF));
    if (number_of_cpu_cores < 1) {
        number_of_cpu_cores = 1;
    }
}

#endif

unsigned int raptor_get_number_of_cpu_cores() {
    static raptor_once_t once = RAPTOR_ONCE_INIT;
    RaptorOnceInit(&once, get_number_of_cpu_cores_impl);
    return static_cast<unsigned int>(number_of_cpu_cores);
}
