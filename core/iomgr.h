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

#ifndef __RAPTOR_CORE_IOMGR__
#define __RAPTOR_CORE_IOMGR__

#include "util/list_entry.h"

namespace raptor {
typedef struct {
    char* name;
    list_entry entry;
} IomgrObject;

void IomgrInit();
void IomgrRegisterObject(IomgrObject* obj, const char* name);
void IomgrUnregisterObject(IomgrObject* obj);
void IomgrShutdown();
} // namespace raptor

#endif  // __RAPTOR_CORE_IOMGR__
