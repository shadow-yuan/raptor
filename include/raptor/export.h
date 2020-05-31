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


#ifndef __RAPTOR_EXPORT_H__
#define __RAPTOR_EXPORT_H__

#ifndef RAPTOR_API

#ifdef RAPTOR_SHARED_LIBRARY
#ifdef _WIN32

#ifdef RAPTOR_COMPILE_LIBRARY
#define RAPTOR_API __declspec(dllexport)
#else
#define RAPTOR_API __declspec(dllimport)
#endif

#else  // _WIN32

#ifdef RAPTOR_COMPILE_LIBRARY
#define RAPTOR_API __attribute__((visibility("default")))
#else
#define RAPTOR_API
#endif

#endif  // _WIN32

#else  // else(RAPTOR_SHARED_LIBRARY)
#define RAPTOR_API
#endif // RAPTOR_SHARED_LIBRARY

#endif  // RAPTOR_API

#endif  // __RAPTOR_EXPORT_H__
