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

#include "util/log.h"
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "util/alloc.h"
#include "util/atomic.h"
#include "util/time.h"

#ifdef _WIN32
#include <processthreadsapi.h>
#else
#include <pthread.h>
#endif

namespace raptor {
namespace {

void log_default_print(LogArgument* args);

AtomicIntptr g_log_function((intptr_t)log_default_print);
AtomicIntptr g_min_level((intptr_t)LogLevel::kLogLevelDebug);
char g_level_string[static_cast<int>(LogLevel::kLogLevelDisable)];

void log_default_print(LogArgument* args) {
#ifdef _WIN32
    static __declspec(thread) unsigned long tid = 0;
    if (tid == 0) tid = GetCurrentThreadId();
    constexpr char delimiter = '\\';
#else
    static __thread unsigned long tid = 0;
    if (tid == 0) tid = static_cast<unsigned long>(pthread_self());
    constexpr char delimiter = '/';
#endif
    const char* last_slash = NULL;
    const char* display_file = NULL;
    char time_buffer[64] = { 0 };

    last_slash = strrchr(args->file, delimiter);
    if (last_slash == NULL) {
        display_file = args->file;
    } else {
        display_file = last_slash + 1;
    }

    struct timeval now;
    gettimeofday(&now, nullptr);
    time_t timer = now.tv_sec;

#ifdef _WIN32
    struct tm stm;
    if (localtime_s(&stm, &timer)) {
        strcpy(time_buffer, "error:localtime");
    }
#else
    struct tm stm;
    if (!localtime_r(&timer, &stm)) {
        strcpy(time_buffer, "error:localtime");
    }
#endif
    // "%F %T" 2020-05-10 01:43:06
    else if (0 == strftime(time_buffer, sizeof(time_buffer), "%F %T", &stm)) {
        strcpy(time_buffer, "error:strftime");
    }

    fprintf(stderr,
        "[%s.%03u %5lu %c] %s (%s:%d)\n",
        time_buffer,
        (int)(now.tv_usec / 1000),  // millisecond
        tid,
        g_level_string[static_cast<int>(args->level)],
        args->message,
        display_file, args->line);

    fflush(stderr);
}
}  // namespace

// ---------------------------

void LogInit(void) {
#ifndef NDEBUG
    LogSetLevel(LogLevel::kLogLevelDebug);
#else
    LogSetLevel(LogLevel::kLogLevelError);
#endif
    g_level_string[static_cast<int>(LogLevel::kLogLevelDebug)] = 'D';
    g_level_string[static_cast<int>(LogLevel::kLogLevelInfo)] = 'I';
    g_level_string[static_cast<int>(LogLevel::kLogLevelError)] = 'E';
}

void LogSetLevel(LogLevel level) {
    g_min_level.Store((intptr_t)level);
}

void LogSetTransferFunction(LogTransferFunction func) {
    g_log_function.Store((intptr_t)func);
}

void LogRestoreDefault() {
    g_log_function.Store((intptr_t)log_default_print);
}

void LogFormatPrint(
                    const char* file,
                    int line,
                    LogLevel level,
                    const char* format, ...) {

    char* message = NULL;
    va_list args;
    va_start(args, format);

#ifdef _WIN32
    int ret = _vscprintf(format, args);
    va_end(args);
    if (ret < 0) {
        return;
    }

    size_t buff_len = (size_t)ret + 1;
    message = (char*)Malloc(buff_len);
    va_start(args, format);
    ret = vsnprintf_s(message, buff_len, _TRUNCATE, format, args);
    va_end(args);
#else
    if (vasprintf(&message, format, args) == -1) {  // stdio.h
        va_end(args);
        return;
    }
#endif

    if (g_min_level.Load() <= static_cast<intptr_t>(level)) {
        LogArgument tmp;
        tmp.file = file;
        tmp.line = line;
        tmp.level = level;
        tmp.message = message;
        ((LogTransferFunction)g_log_function.Load())(&tmp);
    }
    Free(message);
}
} // namespace raptor
