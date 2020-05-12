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

#ifndef __RAPTOR_UTIL_LIST_ENTRY__
#define __RAPTOR_UTIL_LIST_ENTRY__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct list_entry list_entry;

struct list_entry {
    list_entry* prev;
    list_entry* next;
};

#define RAPTOR_LIST_IS_EMPTY(head) \
    ((head)->next == (head))

#define RAPTOR_LIST_ENTRY_INIT(entry) \
    (entry)->prev = (entry)->next = 0

#define RAPTOR_LIST_INIT(head) \
    (head)->prev = (head)->next = (head)

void raptor_list_push_front(list_entry* head, list_entry* new_item);
void raptor_list_push_back(list_entry* head, list_entry* new_item);

list_entry* raptor_list_pop_front(list_entry* head);
list_entry* raptor_list_pop_back(list_entry* head);

// Return 1 on success, 0 on failure
int raptor_list_remove_entry(list_entry* item);

#ifdef __cplusplus
}
#endif

#endif  // __RAPTOR_UTIL_LIST_ENTRY__
