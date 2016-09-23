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

#include "mem.h"

#include <stdlib.h>
#include <string.h>

static int mem_read(pblog_flash_ops *ops, int offset, size_t len, void *data) {
  unsigned char *addr = ops->priv;

  memcpy(data, addr + offset, len);
  return len;
}

static int mem_write(pblog_flash_ops *ops, int offset, size_t len,
                     const void *data) {
  unsigned char *addr = ops->priv;

  memcpy(addr + offset, data, len);
  return len;
}

static int mem_erase(pblog_flash_ops *ops, int offset, size_t len) {
  unsigned char *addr = ops->priv;

  memset(addr + offset, 0xff, len);
  return 0;
}

struct pblog_flash_ops pblog_mem_ops = {
  .read = &mem_read,
  .write = &mem_write,
  .erase = &mem_erase,
  .priv = NULL  /* set to memory address base upon instantiation */
};
