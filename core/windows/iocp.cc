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

#include "core/windows/iocp.h"

namespace raptor {
Iocp::Iocp()
    : _handle(NULL) {
}

Iocp::~Iocp() {
    if (_handle) {
        CloseHandle(_handle);
    }
}

RefCountedPtr<Status> Iocp::create(DWORD max_threads) {
    if (!_handle) {
        _handle =  CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, max_threads);
        if (_handle == NULL) {
            return RAPTOR_WINDOWS_ERROR(GetLastError(), "CreateIoCompletionPort");
        }
    }
    return RAPTOR_ERROR_NONE;
}

bool Iocp::add(SOCKET sock, void* CompletionKey) {
    if (!_handle) return false;
    HANDLE h = CreateIoCompletionPort((HANDLE)sock, _handle, (ULONG_PTR)CompletionKey, 0);
    return (h != NULL);
}

bool Iocp::polling(
        DWORD* NumberOfBytesTransferred,
        PULONG_PTR CompletionKey,
        LPOVERLAPPED *lpOverlapped, DWORD timeout_ms) {
    BOOL r = GetQueuedCompletionStatus(
        _handle, NumberOfBytesTransferred, CompletionKey, lpOverlapped, timeout_ms);
    return (r != FALSE);
}

void Iocp::post(void* key, LPOVERLAPPED overlapped) {
    PostQueuedCompletionStatus(_handle, 0, (ULONG_PTR)key, overlapped);
}
} // namespace raptor
