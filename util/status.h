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

#ifndef __RAPTOR_UTIL_STATUS__
#define __RAPTOR_UTIL_STATUS__

#include <string>

#include "util/alloc.h"
#include "util/string.h"
#include "util/ref_counted.h"

namespace raptor {
class Status final : public RefCounted<Status, NonPolymorphicRefCount> {
public:
    Status();
    Status(const std::string& msg);
    Status(int err_code, const std::string& err_msg);

    Status(const Status& oth);
    Status& operator= (const Status& oth);
    Status(Status&& other);
    Status& operator=(Status&& other);
    ~Status() = default;

    bool IsOK() const;
    int ErrorCode() const;
    std::string ToString() const;
    void AppendMessage(const std::string& msg);

    bool operator==(const Status& other) const;
    bool operator!=(const Status& other) const;

 private:
    // 0: no error
    int _error_code;
    std::string _message;

    friend RefCountedPtr<Status> MakeStatusFromStaticString(const char* msg);
    friend RefCountedPtr<Status> MakeStatusFromPosixError(const char* api);

#ifdef _WIN32
    friend RefCountedPtr<Status> MakeStatusFromWindowsError(int err, const char* api);
#endif
};

RefCountedPtr<Status> MakeStatusFromStaticString(const char* msg);
RefCountedPtr<Status> MakeStatusFromPosixError(const char* api);

#ifdef _WIN32
RefCountedPtr<Status> MakeStatusFromWindowsError(int err, const char* api);
#endif

template <typename... Args>
inline RefCountedPtr<Status> MakeStatusFromFormat(Args&&... args) {
    char* message = nullptr;
    raptor_asprintf(&message, std::forward<Args>(args)...);
    if (!message) {
        return nullptr;
    }
    RefCountedPtr<Status> obj = MakeRefCounted<Status>(message);
    raptor::Free(message);
    return obj;
}

}  // namespace raptor

#define RAPTOR_POSIX_ERROR(api_name) \
    raptor::MakeStatusFromPosixError(api_name)

#define RAPTOR_ERROR_FROM_STATIC_STRING(message) \
    raptor::MakeStatusFromStaticString(message)

#define RAPTOR_ERROR_FROM_FORMAT(FMT, ...) \
    raptor::MakeStatusFromFormat(FMT, ##__VA_ARGS__)

#ifdef _WIN32
#define RAPTOR_WINDOWS_ERROR(err, api_name) \
    raptor::MakeStatusFromWindowsError(err, api_name)
#endif

using raptor_error = raptor::RefCountedPtr<raptor::Status>;
#define RAPTOR_ERROR_NONE nullptr

#endif  // __RAPTOR_UTIL_STATUS__
