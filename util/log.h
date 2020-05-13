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

#ifndef __RAPTOR_LOG__
#define __RAPTOR_LOG__

#include <stdlib.h>
#include "util/useful.h"

namespace raptor {
enum class LogLevel : int {
    kLogLevelDebug,
    kLogLevelInfo,
    kLogLevelError,
    kLogLevelDisable
};

typedef struct {
    const char* file;
    int line;
    LogLevel level;
    const char* message;
} LogArgument;

typedef void (*LogTransferFunction)(LogArgument* arg);

void LogInit(void);
void LogSetLevel(LogLevel level);
void LogFormatPrint(
                    const char* file,
                    int line,
                    LogLevel level,
                    const char* format, ...);

void LogSetTransferFunction(LogTransferFunction func);
void LogRestoreDefault();

} // namespace raptor

#define log_debug(FMT, ...) \
    raptor::LogFormatPrint(__FILE__, __LINE__, raptor::LogLevel::kLogLevelDebug, FMT, ##__VA_ARGS__)

#define log_info(FMT, ...)  \
    raptor::LogFormatPrint(__FILE__, __LINE__, raptor::LogLevel::kLogLevelInfo,  FMT, ##__VA_ARGS__)

#define log_error(FMT, ...) \
    raptor::LogFormatPrint(__FILE__, __LINE__, raptor::LogLevel::kLogLevelError, FMT, ##__VA_ARGS__)

#define RAPTOR_ASSERT(x)                       \
do {                                           \
    if (RAPTOR_UNLIKELY(!(x))) {               \
        log_error("assertion failed: %s", #x); \
        abort();                               \
    }                                          \
} while (0)

#endif  // __RAPTOR_LOG__
