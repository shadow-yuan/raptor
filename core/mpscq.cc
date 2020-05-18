/*
 *
 * Copyright 2016 gRPC authors.
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

#include "core/mpscq.h"
#include "util/log.h"

namespace raptor {

MultiProducerSingleConsumerQueue::MultiProducerSingleConsumerQueue()
    : _newest{&_stub}, _oldest(&_stub) {
}

MultiProducerSingleConsumerQueue::~MultiProducerSingleConsumerQueue() {
    auto p = _newest.Load();
    RAPTOR_ASSERT(p == &_stub);
    RAPTOR_ASSERT(_oldest == &_stub);
}

bool MultiProducerSingleConsumerQueue::push(Node* node) {
    // Oldest -> prev -> newest(node) -> null
    node->next.Store(nullptr);
    Node* prev = _newest.Exchange(node, MemoryOrder::ACQ_REL);
    prev->next.Store(node, MemoryOrder::RELEASE);
    return prev == &_stub;
}

MultiProducerSingleConsumerQueue::Node* MultiProducerSingleConsumerQueue::pop() {
    bool empty = false;
    return PopAndCheckEnd(&empty);
}

MultiProducerSingleConsumerQueue::Node* MultiProducerSingleConsumerQueue::PopAndCheckEnd(bool* empty) {
    Node* tail = _oldest;
    Node* obj = _oldest->next.Load(MemoryOrder::ACQUIRE);
    if (tail == &_stub) {
        // first to pop, first element is in _stub->next
        // the list is empty
        if (obj == nullptr) {
            *empty = true;
            return nullptr;
        }
        // Get the earliest inserted node,
        // and move _oldest pointer
        _oldest = obj;
        tail = obj;
        obj = tail->next.Load(MemoryOrder::ACQUIRE);
    }
    // else // Not the first to pop

    // tail is the last one (oldest)
    // obj is the penultimate

    if (obj != nullptr) {
        *empty = false;
        _oldest = obj;
        return tail;
    }

    // reach the end of the list
    Node* head = _newest.Load(MemoryOrder::ACQUIRE);
    if (tail != head) {
        *empty = false;
        // we're still adding
        return nullptr;
    }
    // only one element left in the list
    // we need to reinitialize the list
    push(&_stub);
    obj = tail->next.Load(MemoryOrder::ACQUIRE);
    if (obj != nullptr) {
        *empty = false;
        _oldest = obj;
        return tail;
    }
    // maybe? we're still adding
    *empty = false;
    return nullptr;
}

} // namespace raptor
