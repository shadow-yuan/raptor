
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

#pragma once

#include "util/atomic.h"

namespace raptor {

// Multiple producer single consumer queue
// If you want to use pop in multiple threads, use LockedMessageQueue
class MultiProducerSingleConsumerQueue final {
public:
    struct Node {
        /* data */
        Atomic<Node*> next;
    };
    MultiProducerSingleConsumerQueue() ;//: _head{&_stub}, _tail(&_stub){}
    ~MultiProducerSingleConsumerQueue();

    // Return true if it's the first element,
    // otherwise return false;
    bool push(Node* node);

    Node* pop();
    Node* PopAndCheckEnd(bool * empty);

private:
    union {
        char _padding[64];
        Atomic<Node*> _newest;
    };
    Node* _oldest;
    Node  _stub;
};

} // namespace raptor
