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

#pragma once
#include <winsock2.h>

namespace raptor {
class Iocp final {
public:
    Iocp();
    ~Iocp();
    bool create(DWORD max_threads = 0);
    bool add(SOCKET sock, void* CompletionKey);
    bool polling(
        DWORD* NumberOfBytesTransferred,
        PULONG_PTR CompletionKey,
        LPOVERLAPPED *lpOverlapped,
        DWORD timeout_ms = 1000);
    void post(void* CompletionKey, LPOVERLAPPED Overlapped);

private:
    HANDLE _handle;
};
} // namespace raptor