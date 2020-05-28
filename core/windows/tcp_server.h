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
#include <stdint.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <map>

#include "core/resolve_address.h"
#include "core/windows/iocp.h"
#include "core/windows/tcp_listen.h"
#include "core/cid.h"
#include "core/windows/connection.h"
#include "core/mpscq.h"
#include "core/windows/iocp_thread.h"
#include "util/sync.h"
#include "util/thread.h"
#include "util/time.h"
#include "util/status.h"
#include "raptor/service.h"
#include "raptor/protocol.h"
#include "util/atomic.h"

namespace raptor {
class Slice;
class TcpListener;
struct TcpMessageNode;
class TcpServer : public internal::IAcceptor
                , public internal::IIocpReceiver
                , public internal::INotificationTransfer {
public:
    TcpServer(ITcpServerService *service, Protocol* proto);
    ~TcpServer();

    raptor_error Init(const RaptorOptions* options);
    raptor_error AddListening(const std::string& addr);
    raptor_error Start();
    void Shutdown();

    bool Send(ConnectionId cid, const void* buf, size_t len);
    bool CloseConnection(ConnectionId cid);

    // internal::IAcceptor impl
    void OnNewConnection(
        SOCKET sock, int listen_port,
        const raptor_resolved_address* addr) override;

    // internal::IIocpReceiver impl
    void OnErrorEvent(void* ptr, size_t err_code) override;
    void OnRecvEvent(void* ptr, size_t transferred_bytes) override;
    void OnSendEvent(void* ptr, size_t transferred_bytes) override;

    // internal::INotificationTransfer impl
    void OnConnectionArrived(ConnectionId cid, const raptor_resolved_address* addr) override;
    void OnDataReceived(ConnectionId cid, const Slice* s) override;
    void OnConnectionClosed(ConnectionId cid) override;

    // user data
    bool SetUserData(ConnectionId cid, void* ptr);
    bool GetUserData(ConnectionId cid, void** ptr) const;
    bool SetExtendInfo(ConnectionId cid, uint64_t data);
    bool GetExtendInfo(ConnectionId cid, uint64_t& data) const;

private:

    void TimeoutCheckThread(void*);
    void MessageQueueThread(void*);
    uint32_t CheckConnectionId(ConnectionId cid) const;
    void Dispatch(struct TcpMessageNode* msg);

private:

    using TimeoutRecord = std::multimap<time_t, uint32_t>;
    struct ConnectionData {
        Connection* con;
        TimeoutRecord::iterator iter;
        ConnectionData() {
            con = nullptr;
        }
    };

    enum { RESERVED_CONNECTION_COUNT = 100 };

    ITcpServerService* _service;
    Protocol* _proto;

    bool _shutdown;
    RaptorOptions _options;

    MultiProducerSingleConsumerQueue _mpscq;
    Thread _mq_thd;
    Mutex _mutex;
    ConditionVariable _cv;
    AtomicUInt32 _count;

    std::shared_ptr<SendRecvThread> _rs_thread;
    std::shared_ptr<TcpListener> _listener;
    Thread _timeout_thread;

    Mutex _conn_mtx;
    std::vector<ConnectionData> _mgr;
    // key: timeout deadline, value: index for _mgr
    TimeoutRecord _timeout_record_list;
    std::list<uint32_t> _free_index_list;
    uint16_t _magic_number;
};
} // namespace raptor
