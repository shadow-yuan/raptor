#pragma once
#include <list>
#include <map>
#include <utility>

#include "util/atomic.h"
#include "util/sync.h"
#include "util/time.h"
#include "raptor/types.h"

namespace raptor {
class Connection;
class ConnectionList {
public:
    ConnectionList();
    ~ConnectionList();
    void Init(size_t max_connections);
    void Add(Connection* c, time_t timeout_time);
    Connection* GetConnection(ConnectionId cid);
    uint32_t GetUniqueId();
    uint16_t GetMagicNumber();
    void Remove(ConnectionId cid);

private:
    using ConnectionData =
        std::pair<Connection*, std::multimap<time_t, uint32_t>::iterator>;

    Mutex _mtx; // for _mgr
    time_t _last_timeout_time;
    uint16_t _magic_number;
    AtomicInt32 _current_index;
    ConnectionData* _conn_vector;
    std::list<uint32_t> _free_list;
    size_t _free_count;
};
}
