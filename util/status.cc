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

#include "util/status.h"
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#endif
#include <utility>

namespace raptor {
Status::Status()
    : Status(0, std::string()) {}

Status::Status(const std::string& msg)
    : Status(-1, msg) {
}

Status::Status(int err_code, const std::string& err_msg)
    : _error_code(err_code), _message(err_msg) {
}

Status::Status(const Status& oth)
    : _error_code(oth._error_code), _message(oth._message)
{}

Status& Status::operator= (const Status& oth) {
    if (this != &oth) {
        _error_code = oth._error_code;
        _message = oth._message;
    }
    return *this;
}

Status::Status(Status&& other) {
    _error_code = std::move(other._error_code);
    _message = std::move(other._message);
}

Status& Status::operator=(Status&& other) {
    _error_code = std::move(other._error_code);
    _message = std::move(other._message);
    return *this;
}

bool Status::IsOK() const {
    return _error_code == 0;
}

int Status::ErrorCode() const {
    return _error_code;
}

std::string Status::ToString() const {
    if (_error_code == 0) {
        return std::string("No error");
    }

    char* text = nullptr;
    raptor_asprintf(&text, "%s: error code is %d.", _message.c_str(), _error_code);
    std::string rsp(text);
    raptor::Free(text);
    return rsp;
}

void Status::AppendMessage(const std::string& msg) {
    _message.append(msg);
}

bool Status::operator==(const Status& other) const {
    return _error_code == other.ErrorCode();
}

bool Status::operator!=(const Status& other) const {
    return _error_code != other.ErrorCode();
}

RefCountedPtr<Status> MakeStatusFromStaticString(const char* msg) {
    return MakeRefCounted<Status>(msg);
}

RefCountedPtr<Status> MakeStatusFromPosixError(const char* api) {
    int err = errno;
    char* text = nullptr;
    raptor_asprintf(&text, "%s: %s", api, strerror(err));
    RefCountedPtr<Status> obj = MakeRefCounted<Status>(err, text);
    raptor::Free(text);
    return obj;
}

#ifdef _WIN32
RefCountedPtr<Status> MakeStatusFromWindowsError(int err, const char* api) {
    char* message = raptor_format_message(err);

    char* text = nullptr;
    raptor_asprintf(&text, "%s: %s", api, message);

    RefCountedPtr<Status> obj = MakeRefCounted<Status>(err, text);
    raptor::Free(message);
    raptor::Free(text);
    return obj;
}
#endif

}  // namespace raptor
