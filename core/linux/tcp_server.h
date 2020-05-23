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
#include <memory>
#include <vector>
#include <list>
#include <map>
#include <utility>

#include "core/linux/epoll_thread.h"
#include "core/linux/connection.h"
#include "core/mpscq.h"
#include "util/status.h"
#include "util/sync.h"
#include "raptor/service.h"

namespace raptor {
class TcpListener;
struct TcpMessageNode;
class TcpServer : public internal::IAcceptor
                , public internal::IEpollReceiver
                , public internal::INotificationTransfer {
public:
    explicit TcpServer(IRaptorServerService* service);
    ~TcpServer();

    RefCountedPtr<Status> Init(const RaptorOptions& options);
    RefCountedPtr<Status> AddListeningPort(const char* addr);

    bool Start();
    bool Shutdown();

    // IAcceptor implement
    void OnNewConnection(int sock_fd,
        int listen_port, const raptor_resolved_address* addr) override;

    // IEpollReceiver implement (epoll event)
    void OnError(void* ptr) override;
    void OnRecvEvent(void* ptr) override;
    void OnSendEvent(void* ptr) override;

    // INotificationTransfer implement
    void OnConnectionArrived(ConnectionId cid, const raptor_resolved_address* addr);
    void OnDataReceived(ConnectionId cid, const Slice* s) override;
    void OnConnectionClosed(ConnectionId cid) override;

    // send data
    RefCountedPtr<Status> SendData(ConnectionId cid, const void* buf, size_t len);
    void CloseConnection(ConnectionId cid);

    // user data
    bool SetUserData(ConnectionId cid, void* ptr);
    bool GetUserData(ConnectionId cid, void** ptr) const;
    bool SetExtendInfo(ConnectionId cid, uint64_t data);
    bool GetExtendInfo(ConnectionId cid, uint64_t& data) const;

private:
    using ConnectionData =
        std::pair<Connection*, std::multimap<time_t, uint32_t>::iterator>;

    void MessageQueueThread();
    void TimeoutCheckoutThread();

    void Dispatch(struct TcpMessageNode* msg);
    void ChannelDispatch(ConnectionId cid, const Slice& s);
    uint32_t GetIdlePosition();
    uint32_t CheckConnectionId(ConnectionId cid) const;

    IRaptorServerService* _service;
    bool _shutdown;
    Thread _mq_thread;
    AtomicUInt32 _count;
    Mutex _mutex;
    ConditionVariable _cv;
    Thread _timeout_thread;

    Mutex _conn_mtx;
    time_t _last_timeout_time;
    uint16_t _magic_number;
    AtomicInt32 _current_index;
    std::vector<ConnectionData> _conn_mgr;
    std::list<uint32_t> _free_list;
    std::multimap<time_t, uint32_t> _timeout_record;

    MultiProducerSingleConsumerQueue _mq;

    std::shared_ptr<TcpListener> _listener;
    std::shared_ptr<SendRecvThread> _recv_thread;
    std::shared_ptr<SendRecvThread> _send_thread;
    std::shared_ptr<Protocol> _proto;

    RaptorOptions _options;
};

} // namespace raptor
