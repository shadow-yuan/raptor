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
#include "core/iocp/tcp_server.h"
#include "util/alloc.h"

namespace raptor {

struct tcp_server {
    ConnectionId cid;
    raptor_resolved_address addr;
    OVERLAPPED overlapped;
    tcp_server() {
        cid = 0;
        memset(&addr, 0, sizeof(addr));
        memset(&overlapped, 0, sizeof(overlapped));
    }
};

TcpServer::TcpServer(IRaptorServerEvent *service)
    : _service(service) {
}

TcpServer::~TcpServer() {}

raptor_error TcpServer::Init(const RaptorOptions* options){
    if (!_shutdown) {
        return RAPTOR_ERROR_FROM_STATIC_STRING("tcp server already running");
    }

    if (!_iocp.create(0)) {
        return RAPTOR_WSA_ERROR("CreateIoCompletionPort");
    }

    _shutdown = false;

    _thd = Thread("server",
        [](void* param) ->void {
            TcpServer* p = (TcpServer*)param;
            p->WorkThread();
        },
        this);

    return RAPTOR_ERROR_NONE;
}

raptor_error TcpServer::AddListening(const std::string& addr){

    return RAPTOR_ERROR_NONE;
}

raptor_error TcpServer::Start(){
    _thd.Start();
    return RAPTOR_ERROR_NONE;
}

raptor_error TcpServer::Shutdown(){
if (!_shutdown) {
        _shutdown = true;
        _iocp.post(NULL, &_exit);
        _thd.Join();

        for(auto ite = _mgr.begin(); ite != _mgr.end(); ite++){
            if((*ite).get()){
                (*ite)->Shutdown(true);
            }
        }
    }
    return RAPTOR_ERROR_NONE;
}

raptor_error TcpServer::Send(ConnectionId cid, const void* buf, size_t len){

    if(!_mgr[cid].get()){
    }

    _mgr[cid]->Send(buf,len);
    return RAPTOR_ERROR_NONE;
}

raptor_error TcpServer::CloseConnection(ConnectionId cid){
    if(!_mgr[cid].get()){
    }

    _mgr[cid]->Shutdown(true);
    return RAPTOR_ERROR_NONE;
}

void TcpServer::OnNewConnection(
    SOCKET fd, int32_t listen_port,
    const raptor_resolved_address* addr) {

    ConnectionId connId = _mgr.size();
    std::shared_ptr<Connection> ptrConn =  std::make_shared<Connection>(this,  connId);
    ptrConn->Init(fd, addr);

    tcp_server* node = new tcp_server;
    node->addr = *addr;
    node->cid = connId;
    if (!_iocp.add(fd, node)) {
        closesocket(fd);
        return ;
    }

    _mtx.Lock();
    _mgr.push_back(ptrConn);
    _mtx.Unlock();

    _service->OnConnected(connId);

}


void TcpServer::OnMessage(ConnectionId cid, const Slice& s){

   _service->OnDataReceived(cid,s.begin(),s.Length());

}


void TcpServer::OnClose(ConnectionId cid) {
    if(_mgr[cid].get()){
        _mgr[cid].reset();
    }

    _service->OnClosed(cid);

}

void GetMessage(){

}

void TcpServer::WorkThread(){
    while (!_shutdown) {
        DWORD NumberOfBytesTransferred = 0;
        tcp_server* CompletionKey = NULL;
        LPOVERLAPPED lpOverlapped = NULL;
        bool ret = _iocp.polling(
            &NumberOfBytesTransferred, (PULONG_PTR)&CompletionKey, &lpOverlapped, INFINITE);

        if (!ret) {
            continue;
        }

        if(lpOverlapped == &_exit) {  // shutdown
            break;
        }

        internal::OverLappedEx *lpoverlappedEx = (internal::OverLappedEx *)lpOverlapped;

        if(lpoverlappedEx->event == internal::EventType::kSendEvent){
            _mgr[CompletionKey->cid]->OnSendEvent(NumberOfBytesTransferred);
        }
        else if(lpoverlappedEx->event == internal::EventType::kRecvEvent){
            _mgr[CompletionKey->cid]->OnRecvEvent(NumberOfBytesTransferred);

        }

    }
}

raptor_error TcpServer::SetUserData(ConnectionId id, void* userdata) {
    return RAPTOR_ERROR_NONE;
}
raptor_error TcpServer::GetUserData(ConnectionId id, void** userdata) {
    return RAPTOR_ERROR_NONE;
}

} // namespace raptor
