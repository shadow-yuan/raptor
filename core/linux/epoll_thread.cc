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

#include "core/linux/epoll_thread.h"
#include "util/time.h"

namespace raptor {
SendRecvThread::SendRecvThread(internal::IEpollReceiver* rcv)
    : _receiver(rcv), _shutdown(true) {
}

SendRecvThread::~SendRecvThread() {}

RefCountedPtr<Status> SendRecvThread::Init() {
    if (!_shutdown) {
        return RAPTOR_ERROR_NONE;
    }

    _shutdown = false;
    auto e = _epoll.create();
    if (e == RAPTOR_ERROR_NONE) {
        _thd = Thread("send/recv",
            std::bind(&SendRecvThread::DoWork, this, std::placeholders::_1), nullptr);
    }
    return e;
}

bool SendRecvThread::Start() {
    if (!_shutdown) {
        _thd.Start();
        return true;
    }
    return false;
}

void SendRecvThread::Shutdown() {
    if (!_shutdown) {
        _shutdown = true;
        _thd.Join();
    }
}

void SendRecvThread::DoWork(void* ptr) {
    while (!_shutdown) {
        
        time_t current_time = Now();
        _receiver->OnCheckingEvent(current_time);

        int number_of_fd = _epoll.polling();
        if (_shutdown) {
            return;
        }
        if (number_of_fd <= 0) {
            continue;
        }

        for (int i = 0; i < number_of_fd; i++) {
            struct epoll_event* ev = _epoll.get_event(i);

            if (ev->events & EPOLLERR
                || ev->events & EPOLLHUP
                || ev->events & EPOLLRDHUP) {
                _receiver->OnErrorEvent(ev->data.ptr);
                continue;
            }
            if (ev->events & EPOLLIN) {
                _receiver->OnRecvEvent(ev->data.ptr);
            }
            if (ev->events & EPOLLOUT) {
                _receiver->OnSendEvent(ev->data.ptr);
            }
        }
    }
}

int SendRecvThread::Add(int fd, void* data, uint32_t events) {
    return _epoll.add(fd, data, events | EPOLLRDHUP);
}

int SendRecvThread::Modify(int fd, void* data, uint32_t events) {
    return _epoll.modify(fd, data, events | EPOLLRDHUP);
}

int SendRecvThread::Delete(int fd, uint32_t events) {
    return _epoll.remove(fd, events | EPOLLRDHUP);
}

} // namespace raptor
