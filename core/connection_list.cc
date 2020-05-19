#include "core/connection_list.h"
namespace raptor {

ConnectionList::ConnectionList(/* args */) {
}

ConnectionList::~ConnectionList() {
}

void ConnectionList::Init(size_t max_connections) {
    _last_timeout_time = Now();
    _magic_number = _last_timeout_time & 0xffff;
    _current_index.Store(0);
    _free_count = 0;

    _conn_vector = new ConnectionData[max_connections];
    for (size_t i = 0; i < max_connections; i++) {
        _conn_vector[i].first = nullptr;
        _free_list.push_back(i);
        _free_count++;
    }
}
}
