/*
 * Copyright 2014 Google Inc. All Rights Reserved.
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
 */

/* Common functions/definitions */

#ifndef _PBLOG_COMMON_H_
#define _PBLOG_COMMON_H_

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Logging/debugging support.
 *   Args: severity (0 == debug, 1 == error) */
/* The integrating application should define this function */
#ifdef PBLOG_NO_PRINTF
static inline int pblog_printf(int severity, const char *format, ...) {
  return 0;
}
#else
extern int pblog_printf(int severity, const char *format, ...);
#endif

#define PBLOG_XSTR(s) PBLOG_STR(s)
#define PBLOG_STR(s) #s

#ifndef PBLOG_DPRINTF
#define PBLOG_DPRINTF(format, ...) \
  do { \
    pblog_printf(0, __FILE__ ":" PBLOG_XSTR(__LINE__) " " format, ##__VA_ARGS__); \
  } while (0)
#endif

#ifndef PBLOG_ERRF
#define PBLOG_ERRF(format, ...) \
  do { \
    pblog_printf(1, __FILE__ ":" PBLOG_XSTR(__LINE__) " " format, ##__VA_ARGS__); \
  } while (0)
#endif

/* Return values */
enum pblog_status {
  PBLOG_SUCCESS = 0,
  PBLOG_ERR_NO_SPACE = -1,
  PBLOG_ERR_INVALID = -2,
  PBLOG_ERR_CHECKSUM = -3,
  PBLOG_ERR_IO = -4,
};

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif  /* _PBLOG_COMMON_H_ */
