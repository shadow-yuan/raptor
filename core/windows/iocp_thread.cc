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

#include "core/windows/iocp_thread.h"
#include <string.h>
#include "util/log.h"
#include "util/time.h"

namespace raptor {
SendRecvThread::SendRecvThread(internal::IIocpReceiver* service)
    : _service(service)
    , _shutdown(true)
    , _rs_threads(0)
    , _threads(nullptr) {
    memset(&_exit, 0, sizeof(_exit));
}

SendRecvThread::~SendRecvThread() {
    if (!_shutdown) {
        Shutdown();
    }
}

RefCountedPtr<Status> SendRecvThread::Init(size_t rs_threads, size_t kernel_threads) {
    if (!_shutdown) return RAPTOR_ERROR_NONE;

    auto e = _iocp.create(kernel_threads);
    if (e != RAPTOR_ERROR_NONE) {
        return e;
    }

    _shutdown = false;
    _rs_threads = rs_threads;
    _threads = new Thread[rs_threads];
    for (size_t i = 0; i < rs_threads; i++) {
        _threads[i] = Thread("send/recv",
            [](void* param) ->void {
                SendRecvThread* p = (SendRecvThread*)param;
                p->WorkThread();
            },
            this);
    }
    return RAPTOR_ERROR_NONE;
}

bool SendRecvThread::Start() {
    if (!_shutdown) {
        RAPTOR_ASSERT(_threads != nullptr);
        for (size_t i = 0; i < _rs_threads; i++) {
            _threads[i].Start();
        }
        return true;
    }
    return false;
}

void SendRecvThread::Shutdown() {
    if (!_shutdown) {
        _shutdown = true;
        _iocp.post(NULL, &_exit);
        for (size_t i = 0; i < _rs_threads; i++) {
            _threads[i].Join();
        }
        delete[] _threads;
        _threads = nullptr;
    }
}

bool SendRecvThread::Add(SOCKET sock, void* CompletionKey) {
    return _iocp.add(sock, CompletionKey);
}

void SendRecvThread::WorkThread(){
    while (!_shutdown) {

        time_t current_time = Now();
        _service->OnCheckingEvent(current_time);

        DWORD NumberOfBytesTransferred = 0;
        void* CompletionKey = NULL;
        LPOVERLAPPED lpOverlapped = NULL;

        // https://docs.microsoft.com/en-us/windows/win32/api/ioapiset/nf-ioapiset-getqueuedcompletionstatus
        bool ret = _iocp.polling(
            &NumberOfBytesTransferred, (PULONG_PTR)&CompletionKey, &lpOverlapped, INFINITE);

        if (!ret) {

            /*
                If theGetQueuedCompletionStatus function succeeds, it dequeued a completion
                packet for a successful I/O operation from the completion port and has
                stored information in the variables pointed to by the following parameters:
                lpNumberOfBytes, lpCompletionKey, and lpOverlapped. Upon failure (the return
                value is FALSE), those same parameters can contain particular value
                combinations as follows:

                    (1) If *lpOverlapped is NULL, the function did not dequeue a completion
                        packet from the completion port. In this case, the function does not
                        store information in the variables pointed to by the lpNumberOfBytes
                        and lpCompletionKey parameters, and their values are indeterminate.

                    (2) If *lpOverlapped is not NULL and the function dequeues a completion
                        packet for a failed I/O operation from the completion port, the
                        function stores information about the failed operation in the
                        variables pointed to by lpNumberOfBytes, lpCompletionKey, and
                        lpOverlapped. To get extended error information, call GetLastError.
            */

            if (lpOverlapped != NULL && CompletionKey != NULL) {
                // Maybe an error occurred or the connection was closed
                DWORD err_code = GetLastError();
                _service->OnErrorEvent(CompletionKey, static_cast<size_t>(err_code));
            }
            continue;
        }

        // shutdown signal

        if(lpOverlapped == &_exit) {
            break;
        }

        // error
        if (NumberOfBytesTransferred == 0) {
            DWORD err_code = GetLastError();
            _service->OnErrorEvent(CompletionKey, static_cast<size_t>(err_code));
            continue;
        }

        OverLappedEx* ptr = (OverLappedEx*)lpOverlapped;
        if (ptr->event == IocpEventType::kRecvEvent) {
            _service->OnRecvEvent(CompletionKey, NumberOfBytesTransferred);
        }
        if (ptr->event == IocpEventType::kSendEvent) {
            _service->OnSendEvent(CompletionKey, NumberOfBytesTransferred);
        }
    }
}

} // namespace raptor
