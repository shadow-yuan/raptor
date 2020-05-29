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

#ifndef __RAPTOR_CORE_EPOLL_THREAD__
#define __RAPTOR_CORE_EPOLL_THREAD__

#include <stdint.h>
#include "core/linux/epoll.h"
#include "core/service.h"
#include "util/status.h"
#include "util/thread.h"

namespace raptor {
class SendRecvThread final {
public:
    explicit SendRecvThread(internal::IEpollReceiver* rcv);
    ~SendRecvThread();

    RefCountedPtr<Status> Init();
    bool Start();
    void Shutdown();

    int Add(int fd, void* data, uint32_t events);
    int Modify(int fd, void* data, uint32_t events);
    int Delete(int fd, uint32_t events);

private:
    void DoWork(void* ptr);
    internal::IEpollReceiver* _receiver;
    bool _shutdown;
    Epoll _epoll;
    Thread _thd;
};

} // namespace raptor
#endif  // __RAPTOR_CORE_EPOLL_THREAD__
