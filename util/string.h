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

#ifndef __RAPTOR_UTIL_ERROR_STRING__
#define __RAPTOR_UTIL_ERROR_STRING__

#ifdef __cplusplus
extern "C" {
#endif

char* raptor_strdup(const char* src);
int raptor_asprintf(char** strp, const char* format, ...);

#ifdef _WIN32
/** Returns a string allocated with gpr_malloc that contains a UTF-8
 * formatted error message, corresponding to the error messageid.
 * Use in conjunction with GetLastError() et al.
 */
char* raptor_format_message(int messageid);
#endif

#ifdef __cplusplus
}
#endif

#endif  // __RAPTOR_UTIL_ERROR_STRING__
