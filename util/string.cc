/*
 *
 * Copyright 2016 gRPC authors.
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

#include "util/string.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "util/alloc.h"

#ifdef _WIN32
#include <windows.h>
#endif

char* raptor_strdup(const char* src) {
    char* dst;
    size_t len;

    if (!src) {
        return nullptr;
    }

    len = strlen(src) + 1;
    dst = static_cast<char*>(raptor::Malloc(len));

    memcpy(dst, src, len);
    return dst;
}

int raptor_asprintf(char** strp, const char* format, ...) {
    va_list args;
    va_start(args, format);
#ifdef _WIN32
    int ret = _vscprintf(format, args);
#else
    char buf[128] = { 0 };
    int ret = vsnprintf(buf, sizeof(buf), format, args);
#endif
    va_end(args);
    if (ret < 0) {
        *strp = nullptr;
        return -1;
    }

    size_t strp_buflen = static_cast<size_t>(ret) + 1;
    if ((*strp = static_cast<char*>(raptor::Malloc(strp_buflen))) == nullptr) {
        return -1;
    }

    // try again using the larger buffer.
    va_start(args, format);
#ifdef _WIN32
    ret = vsnprintf_s(*strp, strp_buflen, _TRUNCATE, format, args);
#else
    if (strp_buflen <= sizeof(buf)) {
        memcpy(*strp, buf, strp_buflen);
        return ret;
    }
    ret = vsnprintf(*strp, strp_buflen, format, args);
#endif
    va_end(args);
    if (static_cast<size_t>(ret) == strp_buflen - 1) {
        return ret;
    }

    // this should never happen.
    raptor::Free(*strp);
    *strp = nullptr;
    return -1;
}

#ifdef _WIN32

char* raptor_format_message(int messageid) {
    char* error_text = NULL;
    // Use MBCS version of FormatMessage to match return value.
    DWORD status = ::FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, (DWORD)messageid, MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT),
        reinterpret_cast<char*>(&error_text), 0, NULL);
    if (status == 0) return raptor_strdup("Unable to retrieve error string");
    char* message = raptor_strdup(error_text);
    ::LocalFree(error_text);
    return message;
}

#endif
