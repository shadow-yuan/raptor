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
#include "core/linux/epoll_thread.h"
#include "core/linux/connection.h"
#include "util/status.h"

namespace raptor {
class TcpListener;
struct TcpMessageNode;
class TcpServer : public Acceptor
                , public Receiver
                , public internal::TransferInterface {
public:
    explicit TcpServer(ITcpServerService* service);
    ~TcpServer();

    RefCountedPtr<Status> Init();
    RefCountedPtr<Status> AddListeningPort(const char* addr);

    bool Start();
    bool Shutdown();

    // Acceptor implement
    void OnNewConnection(int sock_fd,
        int listen_port, const p2p_resolved_address* addr) override;

    // Receiver implement (epoll event)
    void OnError(void* ptr) override;
    void OnRecvEvent(void* ptr) override;
    void OnSendEvent(void* ptr) override;

    void OnNewConnection(ConnectionId cid, const p2p_resolved_address* addr);

    // internal::ServiceInterface implement
    void OnMessage(ConnectionId cid, const Slice* s) override;
    void OnClose(ConnectionId cid) override;

    // send data
    RefCountedPtr<Status> GeneralSend(ConnectionId cid, const void* buf, size_t len);
    RefCountedPtr<Status> ChannelSend(uint32_t channel_id, ConnectionId cid, const void* buf, size_t len);
    void CloseConnection(ConnectionId cid);

    // user data
    RefCountedPtr<Status> SetUserData(ConnectionId cid, void* ptr);
    void* GetUserData(ConnectionId cid);

private:
    void MessageQueueThread();
    void Dispatch(struct TcpMessageNode* msg);
    void ChannelDispatch(ConnectionId cid, const Slice& s);

    bool _shutdown;

    Thread _thd;
    AtomicUInt32 _count;


    std::shared_ptr<TcpListener> _listener;
    std::shared_ptr<RecvSendThread> _recv_thread;
    std::shared_ptr<RecvSendThread> _send_thread;
    std::shared_ptr<Protocol> _protocol;
    std::vector<std::shared_ptr<Connection>> _conn_mgr;
    AtomicUInt32 _index;
};

} // namespace raptor
