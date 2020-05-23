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

#include "core/linux/epoll.h"
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

namespace raptor {

Epoll::Epoll()
    : _epoll_fd(-1) {
    memset(_events, 0, sizeof(_events));
}

Epoll::~Epoll() {
    if (_epoll_fd != -1) {
        close(_epoll_fd);
    }
}

RefCountedPtr<Status> Epoll::create() {
    if (_epoll_fd > 0) {
        return RAPTOR_ERROR_NONE;
    }

    _epoll_fd = epoll_create(MAX_EPOLL_EVENTS);
    if (_epoll_fd < 0) {
        return RAPTOR_POSIX_ERROR("epoll_create");
    }
    if (fcntl(_epoll_fd, F_SETFD, FD_CLOEXEC) != 0) {
        return RAPTOR_POSIX_ERROR("fcntl");
    }
    return RAPTOR_ERROR_NONE;
}

int Epoll::ctl(int fd, epoll_event* ev, int op) {
    return epoll_ctl(_epoll_fd, op, fd, ev);
}

int Epoll::add(int fd, void* data, uint32_t events) {
    epoll_event ev;
    ev.events = events;
    ev.data.ptr = data;
    return ctl(fd, &ev, EPOLL_CTL_ADD);
}

int Epoll::modify(int fd, void* data, uint32_t events) {
    epoll_event ev;
    ev.events = events;
    ev.data.ptr = data;
    return ctl(fd, &ev, EPOLL_CTL_MOD);
}

int Epoll::remove(int fd, uint32_t events) {
    epoll_event ev;
    ev.events = events;
    ev.data.ptr = 0;
    return ctl(fd, &ev, EPOLL_CTL_DEL);
}

int Epoll::polling(int timeout) {
    return epoll_wait(_epoll_fd, _events, MAX_EPOLL_EVENTS, timeout);
}

struct epoll_event* Epoll::get_event(size_t index) {
    return &_events[index];
}
}  // namespace raptor
