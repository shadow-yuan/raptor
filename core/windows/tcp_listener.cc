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

#include "core/windows/tcp_listener.h"
#include <string.h>
#include "core/socket_util.h"
#include "util/alloc.h"
#include "util/list_entry.h"
#include "util/log.h"
#include "core/windows/socket_options.h"

namespace raptor {
struct tcp_listener {
    list_entry entry;
    SOCKET listen_fd;
    SOCKET new_socket;
    int port;
    raptor_dualstack_mode mode;
    uint8_t addr_buffer[(sizeof(raptor_sockaddr_in6) + 16) * 2];
    raptor_resolved_address addr;
    OVERLAPPED overlapped;
    tcp_listener() {
        RAPTOR_LIST_ENTRY_INIT(&entry);
        listen_fd = 0;
        new_socket = 0;
        port = 0;
        memset(addr_buffer, 0, sizeof(addr_buffer));
        memset(&addr, 0, sizeof(addr));
        memset(&overlapped, 0, sizeof(overlapped));
    }
};

TcpListener::TcpListener(IAcceptor* service)
    : _service(service)
    , _shutdown(true) {
    RAPTOR_LIST_INIT(&_head);
    RaptorMutexInit(&_mutex);
    _AcceptEx = NULL;
    _GetAcceptExSockAddrs = NULL;
}

TcpListener::~TcpListener() {
    RaptorMutexDestroy(&_mutex);
}

bool TcpListener::Init() {
    if (!_shutdown) return true;
    if (!_iocp.create(1)) {
        return false;
    }

    _shutdown = false;

    for (int i = 0; i < 2; i++) {
        _thd[i] = Thread("listen",
            [](void* param) ->void {
                TcpListener* p = (TcpListener*)param;
                p->WorkThread();
            },
            this);
    }

    return true;
}

bool TcpListener::Start() {
    if (_shutdown) return false;
    _thd[0].Start();
    _thd[1].Start();
    return true;
}

bool TcpListener::Shutdown() {
    if (!_shutdown) {
        _shutdown = true;
        _iocp.post(NULL, &_exit);
        _thd[0].Join();
        _thd[1].Join();

        RaptorMutexLock(&_mutex);
        list_entry* entry = _head.next;
        while (entry != &_head) {
            auto obj = reinterpret_cast<tcp_listener*>(entry);
            entry = entry->next;
            closesocket(obj->listen_fd);
            delete obj;
        }
        RAPTOR_LIST_INIT(&_head);
        RaptorMutexUnlock(&_mutex);
    }
	return true;
}

raptor_error TcpListener::AddListeningPort(const raptor_resolved_address* addr) {
    if (_shutdown) return RAPTOR_ERROR_FROM_STATIC_STRING("tcp listener is closed");

    raptor_dualstack_mode mode;
    SOCKET listen_fd = 0;

    raptor_error e = raptor_create_socket(addr, &listen_fd, &mode);
    if (e != RAPTOR_ERROR_NONE) {
        log_error("Failed to create socket: %s", e->ToString().c_str());
        return e;
    }

    if (!_AcceptEx || !_GetAcceptExSockAddrs) {
        e = GetExtensionFunction(listen_fd);
        if (e != RAPTOR_ERROR_NONE) {
            closesocket(listen_fd);
            return e;
        }
    }

    int port = 0;
    e = raptor_tcp_server_prepare_socket(listen_fd, addr, &port);
    if (e != RAPTOR_ERROR_NONE) {
        log_error("Failed to configure socket: %s", e->ToString().c_str());
        return e;
    }

    RaptorMutexLock(&_mutex);
    tcp_listener* node = new tcp_listener;
    node->listen_fd = listen_fd;
    node->port = port;
    node->mode = mode;
    node->addr = *addr;
    if (!_iocp.add(node->listen_fd, node)) {
        closesocket(listen_fd);
        delete node;
        RaptorMutexUnlock(&_mutex);
        return RAPTOR_ERROR_FROM_STATIC_STRING("Failed to bind iocp");
    }
    e = StartAcceptEx(node);
    if(e != RAPTOR_ERROR_NONE) {
        closesocket(listen_fd);
        delete node;
        RaptorMutexUnlock(&_mutex);
        return e;
    }

    raptor_list_push_back(&_head, &node->entry);
    RaptorMutexUnlock(&_mutex);

    char* addr_string = nullptr;
    raptor_sockaddr_to_string(&addr_string, addr, 0);
    log_debug("start listening on %s", addr_string? addr_string : std::to_string(node->port).c_str());
    Free(addr_string);
    return e;
}

raptor_error TcpListener::GetExtensionFunction(SOCKET fd) {

    DWORD NumberofBytes;
    int status = 0;

    if (!_AcceptEx) {
        GUID guid = WSAID_ACCEPTEX;
        status =
                WSAIoctl(fd,
                SIO_GET_EXTENSION_FUNCTION_POINTER, &guid, sizeof(guid),
                &_AcceptEx, sizeof(_AcceptEx), &NumberofBytes, NULL, NULL);

        if (status != 0) {
            _AcceptEx = NULL;
            raptor_error e = RAPTOR_WINDOWS_ERROR(WSAGetLastError(), "WSAIoctl");
            log_error("Failed to get AcceptEx: %s", e->ToString().c_str());
            return e;
        }
    }

    if (!_GetAcceptExSockAddrs) {
        GUID guid = WSAID_GETACCEPTEXSOCKADDRS;
        status =
                WSAIoctl(fd,
                SIO_GET_EXTENSION_FUNCTION_POINTER, &guid, sizeof(guid),
                &_GetAcceptExSockAddrs, sizeof(_GetAcceptExSockAddrs), &NumberofBytes, NULL, NULL);

        if (status != 0) {
            _GetAcceptExSockAddrs = NULL;
            raptor_error e = RAPTOR_WINDOWS_ERROR(WSAGetLastError(), "WSAIoctl");
            log_error("Failed to get GetAcceptexSockAddrs: %s", e->ToString().c_str());
            return e;
        }
    }
    return RAPTOR_ERROR_NONE;
}

void TcpListener::WorkThread() {
    while (!_shutdown) {
        DWORD NumberOfBytesTransferred = 0;
        tcp_listener* CompletionKey = NULL;
        LPOVERLAPPED lpOverlapped = NULL;
        bool ret = _iocp.polling(
            &NumberOfBytesTransferred, (PULONG_PTR)&CompletionKey, &lpOverlapped, INFINITE);

        if (!ret) {
            continue;
        }

        if(lpOverlapped == &_exit) {  // shutdown
            break;
        }

        raptor_resolved_address client;
        ParsingNewConnectionAddress(CompletionKey, &client);
        _service->OnNewConnection(CompletionKey->new_socket, CompletionKey->port, &client);

        raptor_error e = StartAcceptEx(CompletionKey);
        if(e != RAPTOR_ERROR_NONE){
            log_error("prepare next accept error");
        }
    }
}

raptor_error TcpListener::StartAcceptEx(struct tcp_listener* sp) {
    SOCKET sock = INVALID_SOCKET;
    BOOL success;
    DWORD addrlen = sizeof(raptor_sockaddr_in6) + 16;
    DWORD bytes_received = 0;
    raptor_error error = RAPTOR_ERROR_NONE;

    sock = WSASocket(
        ((raptor_sockaddr*)sp->addr.addr)->sa_family,
        SOCK_STREAM,
        IPPROTO_TCP, NULL, 0,
        raptor_get_wsa_socket_flags());

    if (sock == INVALID_SOCKET) {
        error = RAPTOR_WINDOWS_ERROR(WSAGetLastError(), "WSASocket");
        goto failure;
    }

    error = raptor_tcp_prepare_socket(sock);
    if (error != RAPTOR_ERROR_NONE) goto failure;

    /* Start the "accept" asynchronously. */
    success = _AcceptEx(sp->listen_fd, sock, sp->addr_buffer, 0,
                            addrlen, addrlen, &bytes_received,
                            &sp->overlapped);

    /* It is possible to get an accept immediately without delay. However, we
        will still get an IOCP notification for it. So let's just ignore it. */
    if (!success) {
        int last_error = WSAGetLastError();
        if (last_error != ERROR_IO_PENDING) {
            error = RAPTOR_WINDOWS_ERROR(WSAGetLastError(), "AcceptEx");
            goto failure;
        }
    }

    // We're ready to do the accept.
    sp->new_socket = sock;
    return RAPTOR_ERROR_NONE;

failure:
    if (sock != INVALID_SOCKET) {
        closesocket(sock);
    }
    return error;
}

void TcpListener::ParsingNewConnectionAddress(
    const tcp_listener* sp, raptor_resolved_address* client) {

    raptor_sockaddr* local = NULL;
    raptor_sockaddr* remote = NULL;

    int local_addr_len = sizeof(raptor_sockaddr_in6) + 16;
    int remote_addr_len = sizeof(raptor_sockaddr_in6) + 16;

    _GetAcceptExSockAddrs(
        (void*)sp->addr_buffer, 0,
        sizeof(raptor_sockaddr_in6) + 16,
        sizeof(raptor_sockaddr_in6) + 16,
        &local, &local_addr_len,
        &remote, &remote_addr_len);

    if(remote != nullptr) {
        client->len = remote_addr_len;
        memcpy(client->addr, remote, remote_addr_len);
    }
}
} // namespace raptor
