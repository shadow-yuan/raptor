#pragma once
#include "raptor/types.h"

class IRaptorServerService {
public:
    virtual ~IRaptorServerService() {}
    virtual void OnConnected(ConnectionId cid) = 0;
    virtual void OnMessageReceived(ConnectionId cid, const void* s, size_t len) = 0;
    virtual void OnClosed(ConnectionId cid) = 0;
};

class IRaptorServerEngine {
public:
    ~IRaptorServerEngine();
    virtual bool Init(const RaptorOptions* options, char* err_msg) = 0;
    virtual bool AddListening(const char* addr, char* err_msg) = 0;
    virtual bool Start(char* err_msg ) = 0;
    virtual bool Shutdown(char* err_msg) = 0;
    virtual bool Send(ConnectionId cid, const void* buff, size_t len, char* err_msg) = 0;
    virtual bool CloseConnection(ConnectionId cid, char* err_msg) = 0;
};
