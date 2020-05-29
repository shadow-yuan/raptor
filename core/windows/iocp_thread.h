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

#ifndef __RAPTOR_CORE_WINDOWS_IOCP_THREAD__
#define __RAPTOR_CORE_WINDOWS_IOCP_THREAD__

#include "core/service.h"
#include "core/windows/iocp.h"
#include "util/status.h"
#include "util/thread.h"

namespace raptor {
class SendRecvThread {
public:
    explicit SendRecvThread(internal::IIocpReceiver* service);
    ~SendRecvThread();
    RefCountedPtr<Status> Init(size_t rs_threads, size_t kernel_threads);
    bool Start();
    void Shutdown();
    bool Add(SOCKET sock, void* CompletionKey);

private:
    void WorkThread();

    internal::IIocpReceiver* _service;
    bool _shutdown;
    size_t _rs_threads;
    Thread* _threads;

    OVERLAPPED _exit;
    Iocp _iocp;
};
} // namespace raptor


#endif  // __RAPTOR_CORE_WINDOWS_IOCP_THREAD__
