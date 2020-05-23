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

#ifndef __RAPTOR_CORE_EPOLL__
#define __RAPTOR_CORE_EPOLL__

#include <sys/epoll.h>
#include <stddef.h>
#include "util/status.h"

namespace raptor {
class Epoll final {
public:
    Epoll();
    ~Epoll();

    RefCountedPtr<Status> create();
    int add(int fd, void* data, uint32_t events);
    int modify(int fd, void* data, uint32_t events);
    int remove(int fd, uint32_t events);

    // epoll_wait
    int polling(int timeout = 1000);
    struct epoll_event* get_event(size_t index);

private:
    enum { MAX_EPOLL_EVENTS = 100 };

    int ctl(int fd, epoll_event* ev, int op);
    int _epoll_fd;
    struct epoll_event _events[MAX_EPOLL_EVENTS];
};
}  // namespace raptor
#endif  // __RAPTOR_CORE_EPOLL__
