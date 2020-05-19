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

#include "util/time.h"

#ifdef _WIN32
int gettimeofday(struct timeval *tp, void *tzp) {
    uint64_t ns100;
    FILETIME time;

    tzp = tzp;

    GetSystemTimeAsFileTime(&time);

    ns100 = (((int64_t)time.dwHighDateTime << 32) + time.dwLowDateTime)
        - 116444736000000000LL;
    tp->tv_sec = static_cast<int32_t>(ns100 / 10000000);
    tp->tv_usec = static_cast<int32_t>((ns100 % 10000000) / 10);

    return (0);
}
#endif

int64_t GetCurrentMilliseconds() {
    struct timeval tp;
    gettimeofday(&tp, nullptr);
    int64_t ret = tp.tv_sec;
    return ret * 1000 + tp.tv_usec / 1000;
}

time_t Now() {
    return time(0);
}
