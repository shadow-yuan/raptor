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

#include "util/list_entry.h"

void raptor_list_push_front(list_entry* head, list_entry* new_item) {
    list_entry* first = head->next;
    head->next = new_item;
    new_item->prev = head;
    new_item->next = first;
    first->prev = new_item;
}

void raptor_list_push_back(list_entry* head, list_entry* new_item) {
    list_entry* last = head->prev;
    last->next = new_item;
    new_item->prev = last;
    new_item->next = head;
    head->prev = new_item;
}

list_entry* raptor_list_pop_front(list_entry* head) {
    list_entry* second = 0;
    list_entry* removed = 0;
    if (RAPTOR_LIST_IS_EMPTY(head)) {
        return 0;
    }

    removed = head->next;
    second = removed->next;

    head->next = second;
    second->prev = head;
    return removed;
}

list_entry* raptor_list_pop_back(list_entry* head) {
    list_entry* second = 0;
    list_entry* removed = 0;
    if (RAPTOR_LIST_IS_EMPTY(head)) {
        return 0;
    }

    removed = head->prev;
    second = removed->prev;

    second->next = head;
    head->prev = second;
    return removed;
}

int raptor_list_remove_entry(list_entry* item) {
    if (item->next == item) {  // item is list head, it's empty.
        return 0;
    }

    list_entry* prior = item->prev;
    list_entry* next = item->next;

    prior->next = next;
    next->prev = prior;
    return 1;
}
