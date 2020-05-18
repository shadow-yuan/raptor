#pragma once
#include "raptor/types.h"

class ITcpServerService {
public:
    virtual ~ITcpServerService() {}
    virtual void OnNewConnection(ConnectionId cid, const void* addr /* struct sockaddr */) = 0;
    virtual void OnMessage(ConnectionId cid, const void* s, size_t len) = 0;
    virtual void OnClose(ConnectionId cid) = 0;
};
