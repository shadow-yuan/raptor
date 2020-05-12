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
#include <stdarg.h>
#include <string.h>
#include "util/alloc.h"

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
    int ret;
    char buf[64];
    size_t strp_buflen;

    va_start(args, format);
    ret = vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);
    if (ret < 0) {
        *strp = nullptr;
        return -1;
    }

    strp_buflen = static_cast<size_t>(ret) + 1;
    if ((*strp = static_cast<char*>(raptor::Malloc(strp_buflen))) == nullptr) {
        return -1;
    }

    if (strp_buflen <= sizeof(buf)) {
        memcpy(*strp, buf, strp_buflen);
        return ret;
    }

    // try again using the larger buffer.
    va_start(args, format);
    ret = vsnprintf(*strp, strp_buflen, format, args);
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

#include <tchar.h>
#include <windows.h>

#if defined UNICODE || defined _UNICODE
LPTSTR raptor_char_to_tchar(LPCSTR input) {
    LPTSTR ret;
    int needed = MultiByteToWideChar(CP_UTF8, 0, input, -1, NULL, 0);
    if (needed <= 0) return NULL;
    ret = (LPTSTR)raptor::Malloc((unsigned)needed * sizeof(TCHAR));
    MultiByteToWideChar(CP_UTF8, 0, input, -1, ret, needed);
    return ret;
}

LPSTR raptor_tchar_to_char(LPCTSTR input) {
    LPSTR ret;
    int needed = WideCharToMultiByte(CP_UTF8, 0, input, -1, NULL, 0, NULL, NULL);
    if (needed <= 0) return NULL;
    ret = (LPSTR)raptor::Malloc((unsigned)needed);
    WideCharToMultiByte(CP_UTF8, 0, input, -1, ret, needed, NULL, NULL);
    return ret;
}
#else
LPTSTR raptor_char_to_tchar(LPCTSTR input) { return (LPTSTR)raptor_strdup(input); }
LPSTR raptor_tchar_to_char(LPCTSTR input) { return (LPSTR)raptor_strdup(input); }
#endif

char* raptor_format_message(int messageid) {
    LPTSTR tmessage;
    char* message;
    DWORD status = FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, (DWORD)messageid, MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT),
        (LPTSTR)(&tmessage), 0, NULL);
    if (status == 0) return raptor_strdup("Unable to retrieve error string");
    message = raptor_tchar_to_char(tmessage);
    raptor::Free(tmessage);
    return message;
}

#endif
