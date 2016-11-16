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

/* Log-structured record interface */

#ifndef _PBLOG_RECORD_H_
#define _PBLOG_RECORD_H_

#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

struct pblog_flash_ops;

/* Header used for each record */
typedef struct record_header {
  uint8_t length_msb;
  uint8_t length_lsb;
  uint8_t checksum;
} __attribute__((packed)) record_header;

/* Header used on each erase block region */
typedef struct region_header {
  /* Magic value used to recognize this as a valid region. */
  uint8_t magic[4];
  /* Sequence number of this region in the circular list.  Stored with
   * LSB in sequence[0].  Note: lowest sequence number is the first in the list.
   */
  uint8_t sequence[4];
}  __attribute__((packed)) region_header;

typedef struct record_intf {
  /* Reads a record.
   * Args:
   *   offset: byte offset of record
   *   next_offset: set to the offset of the next record or 0 if at end
   *   len: maximum data length to read, updated with actual read data length
   *   data: data buffer to write
   * Returns:
   *   0 on success, <0 on failure
   *   next_offset set to '0' on end of log
   */
  int (*read_record)(struct record_intf *ri, int offset, int *next_offset,
      size_t *len, void *data);

  /* Appends a record.
   * Args:
   *   len: length of data to append in bytes
   *   data: record data
   * Returns:
   *   number of bytes actually written on success (may be greater than len)
   */
  int (*append)(struct record_intf *ri, size_t len, const void *data);

  /* Returns the number of free bytes for storing records. */
  int (*get_free_space)(struct record_intf *ri);

  /* Clears num_regions of records, starting from the beginning of the record
   * space.  If num_regions == 0 then clears all regions.
   * Returns:
   *   number of bytes cleared on success
   *   <0 on failure
   */
  int (*clear)(struct record_intf *ri, int num_regions);

  void *priv;
} record_intf;

/* Defines an erase block region */
typedef struct record_region {
  uint32_t offset;  /* offset of this region */
  uint32_t size;  /* total size of region in bytes */
  uint32_t used_size;  /* amount of bytes used in this region */
  uint32_t sequence;  /* sequence number */
} __attribute__((packed)) record_region;

/* Initializes a record interface
 * Args:
 *   regions: array of regions to use (will be copied into internal structures)
 */
int record_intf_init(record_intf *ri,
                     const struct record_region *regions,
                     int num_regions,
                     struct pblog_flash_ops *flash);
void record_intf_free(record_intf *ri);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif  /* _PBLOG_RECORD_H_ */
