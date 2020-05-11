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

#ifndef __RAPTOR_UTIL_USEFUL__
#define __RAPTOR_UTIL_USEFUL__

#define RAPTOR_MIN(a, b) ((a) < (b) ? (a) : (b))
#define RAPTOR_MAX(a, b) ((a) > (b) ? (a) : (b))

#define RAPTOR_ARRAY_SIZE(array) (sizeof(array) / sizeof(*(array)))

/** Set the \a n-th bit of \a i (a mutable pointer). */
#define RAPTOR_BIT_SET(i, n) ((*(i)) |= (1u << (n)))

/** Clear the \a n-th bit of \a i (a mutable pointer). */
#define RAPTOR_BIT_CLEAR(i, n) ((*(i)) &= ~(1u << (n)))

/** Get the \a n-th bit of \a i */
#define RAPTOR_BIT_GET(i, n) (((i) & (1u << (n))) != 0)

#ifdef __GNUC__
#define RAPTOR_LIKELY(x) __builtin_expect((x), 1)
#define RAPTOR_UNLIKELY(x) __builtin_expect((x), 0)
#define RAPTOR_MUST_USE_RESULT __attribute__((warn_unused_result))
#else
#define RAPTOR_LIKELY(x) (x)
#define RAPTOR_UNLIKELY(x) (x)
#define RAPTOR_MUST_USE_RESULT
#endif

#endif  // __RAPTOR_UTIL_USEFUL__
