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

#include "core/iomgr.h"
#include <inttypes.h>
#include <stddef.h>
#include "util/alloc.h"
#include "util/log.h"
#include "util/string.h"
#include "util/sync.h"

namespace raptor {
namespace {
int g_shutdown;
raptor_mutex_t g_mutex;
list_entry g_list_head;

size_t IomgrCountObjects() {
    size_t count = 0;
    list_entry* item = g_list_head.next;
    while (item != &g_list_head) {
        item = item->next;
        count++;
    }
    return count;
}

void IomgrDumpObjects(const char* kind) {
    list_entry* item = g_list_head.next;
    while (item != &g_list_head) {
        int offset = offsetof(IomgrObject, entry);
        IomgrObject* obj = reinterpret_cast<IomgrObject*>((char*)item - offset);
        log_debug("%s OBJECT: %s %p", kind, obj->name, obj);
        item = item->next;
    }
}
} // namespace

void IomgrInit() {
    g_shutdown = false;
    RaptorMutexInit(&g_mutex);
    RAPTOR_LIST_INIT(&g_list_head);
}

void IomgrRegisterObject(IomgrObject* obj, const char* name) {
    obj->name = raptor_strdup(name);
    RaptorMutexLock(&g_mutex);
    raptor_list_push_back(&g_list_head, &obj->entry);
    RaptorMutexUnlock(&g_mutex);
}

void IomgrUnregisterObject(IomgrObject* obj) {
    RaptorMutexLock(&g_mutex);
    raptor_list_remove_entry(&obj->entry);
    RaptorMutexUnlock(&g_mutex);
    Free(obj->name);
}

void IomgrShutdown() {
    RaptorMutexLock(&g_mutex);
    g_shutdown = true;

    if (! RAPTOR_LIST_IS_EMPTY(&g_list_head)) {
        log_debug(
            "Failed to free %" PRIuPTR
            " iomgr objects before shutdown: "
            "memory leaks are likely",
            IomgrCountObjects());
        IomgrDumpObjects("LEAKED");
    }

    RaptorMutexUnlock(&g_mutex);
}

} // namespace raptor

