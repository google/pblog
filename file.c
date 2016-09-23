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

#include "file.h"

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

static int file_read(pblog_flash_ops *ops, int offset, size_t len, void *data) {
  const char *filename = ops->priv;

  int fd = open(filename, O_RDONLY);
  if (fd < 0) {
    return -1;
  }

  int rc = pread(fd, data, len, offset);
  close(fd);
  return rc;
}

static int file_write(pblog_flash_ops *ops, int offset, size_t len,
                      const void *data) {
  const char *filename = ops->priv;

  int fd = open(filename, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
  if (fd < 0) {
    return -1;
  }

  int rc = pwrite(fd, data, len, offset);
  close(fd);
  return rc;
}

static int file_erase(pblog_flash_ops *ops, int offset, size_t len) {
  unsigned char *erase_buf = malloc(len);
  memset(erase_buf, 0xff, len);
  int rc = ops->write(ops, offset, len, erase_buf);
  free(erase_buf);
  return rc == len ? 0 : -1;
}

struct pblog_flash_ops pblog_file_ops = {
  .read = &file_read,
  .write = &file_write,
  .erase = &file_erase,
  .priv = NULL  /* filename to be set on instantiation */
};
