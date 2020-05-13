/*
 *
 * Copyright 2015 gRPC authors.
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

#include "core/host_port.h"
#include <string.h>
#include "util/string.h"

namespace raptor {
int JoinHostPort(UniquePtr<char>* out, const char* host, int port) {
    char* tmp;
    int ret;
    if (host[0] != '[' && strchr(host, ':') != nullptr) {
        /* IPv6 literals must be enclosed in brackets. */
        ret = raptor_asprintf(&tmp, "[%s]:%d", host, port);
    } else {
        /* Ordinary non-bracketed host:port. */
        ret = raptor_asprintf(&tmp, "%s:%d", host, port);
    }
    out->reset(tmp);
    return ret;
}

namespace {
bool DoSplitHostPort(StringView name, StringView* host, StringView* port,
                     bool* has_port) {
    *has_port = false;
    if (name[0] == '[') {
        /* Parse a bracketed host, typically an IPv6 literal. */
        const size_t rbracket = name.find(']', 1);
        if (rbracket == StringView::npos) {
            /* Unmatched [ */
            return false;
        }
        if (rbracket == name.size() - 1) {
            /* ]<end> */
            *port = StringView();
        } else if (name[rbracket + 1] == ':') {
            /* ]:<port?> */
            *port = name.substr(rbracket + 2, name.size() - rbracket - 2);
            *has_port = true;
        } else {
            /* ]<invalid> */
            return false;
        }
        *host = name.substr(1, rbracket - 1);
        if (host->find(':') == StringView::npos) {
            /* Require all bracketed hosts to contain a colon, because a hostname or
                IPv4 address should never use brackets. */
            *host = StringView();
            return false;
        }
    } else {
        size_t colon = name.find(':');
        if (colon != StringView::npos &&
            name.find(':', colon + 1) == StringView::npos) {
            /* Exactly 1 colon.  Split into host:port. */
            *host = name.substr(0, colon);
            *port = name.substr(colon + 1, name.size() - colon - 1);
            *has_port = true;
        } else {
            /* 0 or 2+ colons.  Bare hostname or IPv6 litearal. */
            *host = name;
            *port = StringView();
        }
    }
    return true;
}
}  // namespace

bool SplitHostPort(StringView name, StringView* host, StringView* port) {
    bool has_port = false;

    host->clear();
    port->clear();

    return DoSplitHostPort(name, host, port, &has_port);
}

bool SplitHostPort(StringView name, UniquePtr<char>* host, UniquePtr<char>* port) {
    StringView host_view;
    StringView port_view;

    bool has_port = false;
    bool ret = DoSplitHostPort(name, &host_view, &port_view, &has_port);
    if (ret) {
        *host = StringViewToCString(host_view);
        if (has_port) {
            *port = StringViewToCString(port_view);
        }
    }
    return ret;
}

}  // namespace raptor
