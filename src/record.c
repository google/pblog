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

#include <string.h>

#include <pblog/common.h>
#include <pblog/flash.h>
#include <pblog/record.h>

unsigned char record_checksum(const void *buf, size_t len) {
  unsigned char csum = 0;
  const unsigned char *p = buf;
  for (; p < (unsigned char *)(buf) + len; p++) {
    csum += *p;
  }
  return csum;
}

struct log_metadata {
  struct record_region *regions;
  int num_regions;
  int used_regions;  // the number of regions in use
  int head_region;  // the first region (beginning of records)
  int next_sequence;  // next sequence number to use
  struct pblog_flash_ops *flash;
};

const uint8_t record_magic[4] = {'R','E','C',0xfe};

// Helper to return the i-th region starting from the head region.
struct record_region *region_at(struct log_metadata *meta, int i) {
  if (i < 0 || i >= meta->num_regions) {
    return NULL;
  }

  return &meta->regions[(meta->head_region + i) % meta->num_regions];
}

// Reads a record within a region.
// Args:
//   offset: byte offset within region
//   next_offset: set to the offset of the next record
//   len: maximum data length to read, updated with actual read data length
//   data: data buffer to write
static int region_read_record(struct log_metadata *meta,
                              struct record_region *region,
                              int offset, int *next_offset,
                              size_t *len, void *data) {
  int rc;
  record_header header;
  int length;
  int data_length;

  PBLOG_DPRINTF("read rseq %d, offset %d\n", region->sequence, offset);

  *next_offset = 0;

  if (offset > region->size - sizeof(header)) {
    return PBLOG_ERR_INVALID;
  }

  // Read in the record header.
  rc = meta->flash->read(meta->flash, region->offset + offset,
                         sizeof(header), &header);
  if (rc != sizeof(header)) {
    return rc < 0 ? rc : PBLOG_ERR_IO;
  }

  // End of log?
  length = header.length_lsb | (header.length_msb << 8);
  if (length == 0 || length == 0xffff) {
    if (len) {
      *len = 0;
    }
    return PBLOG_SUCCESS;
  }

  data_length = length - sizeof(header);

  if (length > region->size - offset) {
    PBLOG_ERRF("bad record length found at offset %d: %d\n", offset, length);
    return PBLOG_ERR_INVALID;
  }

  *next_offset = length;
  if (len != NULL) {
    // Check we can fit in the user-provided buffer.
    if (data_length > *len) {
      // Return the required length to the user so they can resize the buffer.
      *len = data_length;
      return PBLOG_ERR_NO_SPACE;
    }
    *len = data_length;

    // Read in the record data.
    if (data != NULL) {
      unsigned char checksum;
      rc = meta->flash->read(meta->flash, region->offset + offset
                             + sizeof(header), data_length, data);
      if (rc != data_length) {
        *len = rc > 0 ? rc : 0;
        return rc < 0 ? rc : PBLOG_ERR_IO;
      }
      checksum = record_checksum(&header, sizeof(header)) +
          record_checksum(data, data_length);
      if (checksum != 0) {
        PBLOG_ERRF("checksum failure record off:%d, checksum: %d\n",
                   offset, checksum);
        rc = PBLOG_ERR_CHECKSUM;
      }
    }
  }

  return rc < 0 ? rc : PBLOG_SUCCESS;
}

static int log_read_record(struct record_intf *ri, int offset, int *next_offset,
                           size_t *len, void *data) {
  struct log_metadata *meta = ri->priv;

  // Determine the region that contains this offset.
  int i;
  struct record_region *region = NULL;
  for (i = 0; i < meta->used_regions; ++i) {
    // Account for the region header at the beginning of each region.
    offset += sizeof(struct region_header);

    region = region_at(meta, i);
    if (offset < region->used_size) {
      break;
    }
    offset -= region->used_size;
  }
  if (i >= meta->used_regions) {
    // Check for end of log (reading last record one past end).
    // Return success in that case, set next_offset to 0.
    if (offset == 0 || offset == region->used_size) {
      *next_offset = 0;
      if (len) *len = 0;
      return PBLOG_SUCCESS;
    } else {
      return PBLOG_ERR_INVALID;
    }
  }

  return region_read_record(meta, region, offset, next_offset, len, data);
}

static int region_append(struct log_metadata *meta,
                         struct record_region *region,
                         size_t len, const void *data) {
  int rc;
  record_header header;

  int record_size = len + sizeof(record_header);
  if (record_size > (region->size - region->used_size)) {
    PBLOG_ERRF("region rseq %d full\n", region->sequence);
    return PBLOG_ERR_NO_SPACE;
  }

  // Update and write out the header.
  header.length_lsb = record_size & 0xff;
  header.length_msb = (record_size >> 8) & 0xff;
  // The checksum covers the entire record including the header.
  header.checksum = 0;
  header.checksum = -(record_checksum(&header, sizeof(header)) +
      record_checksum(data, len));

  // Write out the header then record.
  rc = meta->flash->write(meta->flash, region->offset + region->used_size,
                          sizeof(header), &header);
  if (rc != sizeof(header)) {
    PBLOG_ERRF("header write error: %d\n", rc);
    return rc < 0 ? rc : PBLOG_ERR_IO;
  }
  rc = meta->flash->write(meta->flash,
                          region->offset + region->used_size + sizeof(header),
                          len, data);
  if (rc != len) {
    PBLOG_ERRF("data write error: %d\n", rc);
    return rc < 0 ? rc : PBLOG_ERR_IO;
  }

  // Adjust the metadata.
  region->used_size += record_size;
  return record_size;
}

static int log_append(struct record_intf *ri,
                      size_t len, const void *data) {
  struct log_metadata *meta = ri->priv;

  // Check which region we can fit into.
  int record_size = len + sizeof(record_header);

  struct record_region *tail_region = region_at(meta, meta->used_regions - 1);
  // Check if we need to go to the next free region.
  if (record_size > tail_region->size - tail_region->used_size) {
    if (meta->used_regions < meta->num_regions) {
      meta->used_regions++;
      tail_region = region_at(meta, meta->used_regions - 1);
    } else {
      PBLOG_ERRF("log full: %d used regions, %d used bytes in tail\n",
                 meta->used_regions, tail_region->used_size);
      return PBLOG_ERR_NO_SPACE;
    }
  }

  return region_append(meta, tail_region, len, data);
}

static int log_get_free_space(struct record_intf *ri) {
  struct log_metadata *meta = ri->priv;

  int i;
  int free_space = 0;
  for (i = meta->used_regions - 1; i < meta->num_regions; ++i) {
    struct record_region *region = region_at(meta, i);
    free_space += region->size - region->used_size;
  }

  // Subtract out the size of a single record header as we require atleast
  // that much overhead.
  free_space -= sizeof(record_header);
  return free_space < 0 ? 0 : free_space;
}

static int region_create(struct log_metadata *meta,
                         struct record_region *region,
                         uint32_t sequence);

static int log_clear(struct record_intf *ri, int num_to_clear) {
  struct log_metadata *meta = ri->priv;
  int i;
  int freed_space = 0;

  if (num_to_clear > meta->num_regions || num_to_clear == 0) {
    num_to_clear = meta->num_regions;
  }

  for (i = 0; i < num_to_clear; ++i) {
    struct record_region *region = region_at(meta, i);
    const int old_seq = region->sequence;
    int rc;

    freed_space += region->size;
    rc = region_create(meta, region, meta->next_sequence++);
    if (rc != PBLOG_SUCCESS) {
      PBLOG_ERRF("error clearing region %d\n", i);
      return rc;
    }
    (void)old_seq;
    PBLOG_DPRINTF("region %d cleared, old rseq:%d new rseq:%d\n",
                  i, old_seq, region->sequence);
  }

  meta->head_region = (meta->head_region + num_to_clear) % meta->num_regions;
  meta->used_regions -= num_to_clear;
  if (meta->used_regions <= 0) {
    meta->used_regions = 1;
  }

  return freed_space;
}

// Initialize a region for first time use.
static int region_create(struct log_metadata *meta,
                         struct record_region *region,
                         uint32_t sequence) {
  struct region_header header;
  int i;
  int rc;

  rc = meta->flash->erase(meta->flash, region->offset, region->size);
  if (rc != PBLOG_SUCCESS) {
    PBLOG_ERRF("region roff %d erase error: %d\n", region->offset, rc);
    return rc;
  }

  for (i = 0; i < sizeof(header.magic); ++i) {
    header.magic[i] = record_magic[i];
  }
  header.sequence[0] = sequence & 0xff;
  header.sequence[1] = (sequence >> 8) & 0xff;
  header.sequence[2] = (sequence >> 16) & 0xff;
  header.sequence[3] = (sequence >> 24) & 0xff;
  if (region->size < sizeof(header)) {
    PBLOG_ERRF("region roff %d too small\n", region->offset);
    return PBLOG_ERR_NO_SPACE;
  }

  rc = meta->flash->write(meta->flash, region->offset, sizeof(header), &header);
  if (rc != sizeof(header)) {
    PBLOG_ERRF("region roff %d header write error: %d\n", region->offset, rc);
    return rc < 0 ? rc : PBLOG_ERR_IO;
  }

  region->used_size = sizeof(header);
  region->sequence = sequence;

  return PBLOG_SUCCESS;
}

// Reads the number of records in this region to determine the used space.
static int region_calc_used_size(struct log_metadata *meta,
                                 struct record_region *region) {
  int offset = sizeof(struct region_header);
  while (1) {
    int next_offset;
    region_read_record(meta, region, offset, &next_offset, NULL, NULL);
    if (next_offset == 0) {
      break;
    }
    offset += next_offset;
  }
  return offset;
}

// Initializes a single region struct by reading the region header.
// On read failure will create the region.
static int region_init(struct log_metadata *meta,
                       struct record_region *region) {
  int rc;
  struct region_header header;
  uint32_t sequence;

  // Read in the region header.
  rc = meta->flash->read(meta->flash, region->offset, sizeof(header), &header);

  if (rc != sizeof(header)) {
    PBLOG_ERRF("region roff %d header read error: %d\n", region->offset, rc);
    return region_create(meta, region, meta->next_sequence++);
  }

  sequence = header.sequence[0] | header.sequence[1] << 8
      | header.sequence[2] << 16 | header.sequence[3] << 24;

  if (header.magic[0] != record_magic[0] ||
      header.magic[1] != record_magic[1] ||
      header.magic[2] != record_magic[2] ||
      header.magic[3] != record_magic[3]) {
    PBLOG_DPRINTF("region roff %d invalid header: %02x%02x%02x%02x\n",
                  region->offset, header.magic[0], header.magic[1],
                  header.magic[2], header.magic[3]);
    return region_create(meta, region, meta->next_sequence++);
  }

  if (sequence >= meta->next_sequence) {
    meta->next_sequence = sequence + 1;
  }

  region->sequence = sequence;
  region->used_size = region_calc_used_size(meta, region);
  return PBLOG_SUCCESS;
}

// Initializes the head region as the one with the lowest sequence number.
static void record_intf_init_head_region(struct log_metadata *meta) {
  uint32_t min_sequence = 0xffffffff;
  int min_region = 0;
  int i;

  meta->head_region = 0;
  for (i = 0; i < meta->num_regions; ++i) {
    struct record_region *region = region_at(meta, i);
    if (region->sequence < min_sequence) {
      min_sequence = region->sequence;
      min_region = i;
    }
  }

  meta->head_region = min_region;
}

// Initializes the number of used regions as the regions with valid stored
// records.
static void record_intf_init_used_regions(struct log_metadata *meta) {
  int used_regions = 0;
  int i;
  for (i = 0; i < meta->num_regions; ++i) {
    struct record_region *region = region_at(meta, i);
    if (region->used_size > sizeof(struct region_header)) {
      used_regions++;
    } else {
      break;
    }
  }
  // We always have atleast one used region.
  meta->used_regions = used_regions <= 0 ? 1 : used_regions;
}

static int record_intf_init_meta(struct record_intf *log) {
  struct log_metadata *meta = log->priv;
  int i;

  // Determine the number of records in each region.
  for (i = 0; i < meta->num_regions; ++i) {
    int rc = region_init(meta, &meta->regions[i]);
    if (rc < 0) {
      PBLOG_ERRF("region %d init failure, ignoring region\n", i);
      // Mark the size of the region as 0 so we don't try to use it.
      meta->regions[i].size = 0;
      meta->regions[i].used_size = 0;
    }

    PBLOG_DPRINTF(
        "region %d. rseq:%d offset:%d size:%d used_size:%d\n",
        i, meta->regions[i].sequence,
        meta->regions[i].offset, meta->regions[i].size,
        meta->regions[i].used_size);
  }

  record_intf_init_head_region(meta);
  record_intf_init_used_regions(meta);

  PBLOG_DPRINTF(
      "init num_regions:%d used_regions:%d head_region:%d "
      "next_sequence:%d\n", meta->num_regions, meta->used_regions,
      meta->head_region, meta->next_sequence);
  return PBLOG_SUCCESS;
}

int record_intf_init(record_intf *ri,
                     const struct record_region *regions,
                     int num_regions,
                     struct pblog_flash_ops *flash) {
  struct log_metadata *meta;
  if (num_regions < 1) {
    return PBLOG_ERR_INVALID;
  }
  meta = malloc(sizeof(struct log_metadata));

  meta->regions = malloc(sizeof(*regions) * num_regions);
  memcpy(meta->regions, regions, sizeof(*regions) * num_regions);
  meta->num_regions = num_regions;
  meta->next_sequence = 0;

  meta->flash = flash;

  ri->read_record = log_read_record;
  ri->append = log_append;
  ri->get_free_space = log_get_free_space;
  ri->clear = log_clear;

  ri->priv = meta;

  return record_intf_init_meta(ri);
}

void record_intf_free(record_intf *ri) {
  struct log_metadata *meta = ri->priv;
  free(meta->regions);
  free(meta);
}
