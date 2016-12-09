/*
 * Copyright 2014-2016 Google Inc. All Rights Reserved.
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

/* Flash interface */

#ifndef _PBLOG_FLASH_H_
#define _PBLOG_FLASH_H_

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct pblog_flash_ops {
  /* Read/write operations.  Returns number of bytes read/written. */
  int (*read)(struct pblog_flash_ops *ops, int offset, size_t len, void *data);
  int (*write)(struct pblog_flash_ops *ops, int offset, size_t len,
               const void *data);
  /* Erase region.  Returns 0 on success */
  int (*erase)(struct pblog_flash_ops *ops, int offset, size_t len);

  void *priv;
} pblog_flash_ops;

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* _PBLOG_FLASH_H_ */
