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

#include "raptor/c.h"
#include "core/sockaddr.h"
#include "surface/adapter.h"
#include "util/atomic.h"
#include "util/log.h"
#include "util/useful.h"

struct raptor_server_t   { RaptorServerAdapter   * server;  };
struct raptor_client_t   { RaptorClientAdapter   * client;  };
struct raptor_protocol_t { RaptorProtocolAdapter * proto;   };

using namespace raptor;
static Atomic<uintptr_t> g_log_transfer(0);

int raptor_global_init() {
#ifdef _WIN32
    WSADATA wsaData;
    int status = WSAStartup(MAKEWORD(2, 0), &wsaData);
    RAPTOR_ASSERT(status == 0);
#endif
    raptor::LogInit();
    return 0;
}

int raptor_global_cleanup() {
#ifdef _WIN32
    int status = WSACleanup();
    RAPTOR_ASSERT(status == 0);
#endif
    return 0;
}

// ---- log ----
static void internal_log_transfer_proc(LogArgument* arg) {
    raptor_log_callback fcb = (raptor_log_callback)g_log_transfer.Load();
    if (fcb) {
        fcb(arg->file, arg->line, (int)arg->level, arg->message);
    }
}

void raptor_set_log_level(int level /* 0: debug, 1: info, 2: error */) {
    switch (level) {
    case 0:
        LogSetLevel(LogLevel::kLogLevelDebug);
        break;
    case 1:
        LogSetLevel(LogLevel::kLogLevelInfo);
        break;
    case 2:
        LogSetLevel(LogLevel::kLogLevelError);
        break;
    case 3:
        LogSetLevel(LogLevel::kLogLevelDisable);
        break;
    default:
        break;
    }
}

void raptor_set_log_callback(raptor_log_callback cb) {
    if (!cb) {
        LogRestoreDefault();
    } else {
        g_log_transfer.Store((uintptr_t)cb);
        LogSetTransferFunction(internal_log_transfer_proc);
    }
}

// ---- server ----
raptor_server_t*
    raptor_server_create(const raptor_options_t* options) {
    raptor_server_t* s = new raptor_server_t;
    s->server = new RaptorServerAdapter;
    if (s->server->Init(options)) {
        return s;
    }
    delete s->server;
    delete s;
    return NULL;
}

void raptor_server_set_protocol(raptor_server_t* s, raptor_protocol_t* p) {
    if (!s || !p) return;
    s->server->SetProtocol(p->proto);
}

int raptor_server_start(raptor_server_t* s) {
    if (s) {
        return s->server->Start() ? 1 : 0;
    }
    return 0;
}

int raptor_server_listening(raptor_server_t* s, const char* address) {
    if (s) {
        return s->server->AddListening(address) ? 1 : 0;
    }
    return 0;
}

int raptor_server_shutdown(raptor_server_t* s) {
    if (s) {
        s->server->Shutdown();
        return 1;
    }
    return 0;
}

int raptor_server_set_callbacks(
                                raptor_server_t* s,
                                raptor_server_callback_connection_arrived on_arrived,
                                raptor_server_callback_message_received on_message_received,
                                raptor_server_callback_connection_closed on_closed
                                ) {
    if (s) {
        s->server->SetCallbacks(on_arrived, on_message_received, on_closed);
        return 1;
    }
    return 0;
}

int raptor_server_send(
    raptor_server_t* s,
    raptor_connection_t c, const void* data, size_t len) {
    if (s) {
        return s->server->Send(c, data, len) ? 1 : 0;
    }
    return 0;
}

int raptor_server_set_userdata(
    raptor_server_t* s, raptor_connection_t c, void* userdata) {
    if (s) {
        return s->server->SetUserData(c, userdata) ? 1 : 0;
    }
    return 0;
}

int raptor_server_get_userdata(
    raptor_server_t* s, raptor_connection_t c, void** userdata) {
    if (s) {
        return s->server->GetUserData(c, userdata) ? 1 : 0;
    }
    return 0;
}

int raptor_server_set_extend_info(
    raptor_server_t* s, raptor_connection_t c, uint64_t info) {
    if (s) {
        return s->server->SetExtendInfo(c, info) ? 1: 0;
    }
    return 0;
}

int raptor_server_get_extend_info(
    raptor_server_t* s, raptor_connection_t c, uint64_t* info) {
    if (s) {
        return s->server->GetExtendInfo(c, info) ? 1: 0;
    }
    return 0;
}

int raptor_server_close_connection(raptor_server_t* s, raptor_connection_t c) {
    if (s) {
        return s->server->CloseConnection(c) ? 1 : 0;
    }
    return 0;
}

void raptor_server_destroy(raptor_server_t* s) {
    if (s) {
        delete s->server;
        delete s;
    }
}

// ---- client ----

raptor_client_t*
    raptor_client_create() {
    raptor_client_t* c = new raptor_client_t;
    c->client = new RaptorClientAdapter;
    if (c->client->Init()) {
        return c;
    }
    delete c->client;
    delete c;
    return nullptr;
}

void raptor_client_set_protocol(raptor_client_t* s, raptor_protocol_t* p) {
    if (!s || !p) return;
    s->client->SetProtocol(p->proto);
}

int raptor_client_connect(
    raptor_client_t* c, const char* address, size_t timeout_ms) {
    if (c) {
        return c->client->Connect(address, timeout_ms) ? 1 : 0;
    }
    return 0;
}

int raptor_client_shutdown(raptor_client_t* c) {
    if (c) {
        c->client->Shutdown();
        return 1;
    }
    return 0;
}

int raptor_client_set_callbacks(
                                raptor_client_t* c,
                                raptor_client_callback_connect_result on_connect_result,
                                raptor_client_callback_message_received on_message_received,
                                raptor_client_callback_connection_closed on_closed
                                ) {
    if (c) {
        c->client->SetCallbacks(on_connect_result, on_message_received, on_closed);
        return 1;
    }
    return 0;
}

int raptor_client_send(raptor_client_t* c, const void* buff, size_t len) {
    if (c) {
        return c->client->Send(buff, len) ? 1 : 0;
    }
    return 0;
}

void raptor_client_destroy(raptor_client_t* c) {
    if (c) {
        delete c->client;
        delete c;
    }
}

raptor_protocol_t* raptor_protocol_create() {
    raptor_protocol_t* p = new raptor_protocol_t;
    p->proto = new RaptorProtocolAdapter;
    return p;
}

void raptor_protocol_set_callbacks(
                                raptor_protocol_t* p,
                                raptor_protocol_callback_get_max_header_size cb1,
                                raptor_protocol_callback_build_package_header cb2,
                                raptor_protocol_callback_check_package_length cb3
                                ) {
    p->proto->SetCallbacks(cb1, cb2, cb3);
}

void raptor_protocol_destroy(raptor_protocol_t* p) {
    delete p->proto;
    delete p;
}
