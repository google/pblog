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

/* Base support for reading/writing of protobuf log events */

#include <string.h>
#include <stdlib.h>

#include <pblog/common.h>
#include <pblog/event.h>
#include <pblog/flash.h>
#include <pblog/mem.h>
#include <pblog/pblog.h>
#include <pblog/record.h>

struct pblog_metadata {
  struct record_intf *flash_ri;
  struct record_intf *mem_ri;
  int allow_clear_on_add;
};

static int sync_events(struct record_intf *source, struct record_intf *dest);

static int write_event(struct pblog *pblog, pblog_Event *event) {
  struct pblog_metadata *meta = pblog->priv;
  unsigned char event_buf[PBLOG_MAX_EVENT_SIZE];
  int rc;
  int encoded_size;

  // Add current timestamp and bootnum if not set.
  if (!event->has_boot_number && pblog->get_current_bootnum) {
    event->boot_number = pblog->get_current_bootnum(pblog);
    event->has_boot_number = 1;
  }
  if (!event->has_timestamp && pblog->get_time_now) {
    event->timestamp = pblog->get_time_now(pblog);
    event->has_timestamp = 1;
  }

  // Encode the event and determine the size.
  encoded_size = event_encode(event, event_buf, sizeof(event_buf));
  if (encoded_size < 0) {
    return encoded_size;
  }

  rc = meta->flash_ri->append(meta->flash_ri, encoded_size, event_buf);
  if (rc < 0) {
    PBLOG_ERRF("pblog: failed to write event to flash\n");
    return rc;
  }
  if (meta->mem_ri) {
    rc = meta->mem_ri->append(meta->mem_ri, encoded_size, event_buf);
    if (rc < 0) {
      PBLOG_ERRF("pblog: failed to write event to memory\n");
      return rc;
    }
  }

  return PBLOG_SUCCESS;
}

static int write_clear_event(struct pblog *pblog) {
  pblog_Event *event = (pblog_Event*) malloc(sizeof(pblog_Event));
  int ret;

  if (event == NULL) {
    return PBLOG_ERR_NO_SPACE;
  }

  event_init(event);
  event->has_type = true;
  event->type = pblog_TYPE_LOG_CLEARED;
  ret = write_event(pblog, event);
  event_free(event);
  free(event);
  return ret;
}

// Compacts the log by removing the old entries.
static int log_compact(struct pblog *pblog) {
  struct pblog_metadata *meta = pblog->priv;

  // Clear the oldest flash region.
  int rc = meta->flash_ri->clear(meta->flash_ri, 1);
  if (rc < 0) {
    return rc;
  }

  // Clear the entire mem log.
  if (meta->mem_ri) {
    rc = meta->mem_ri->clear(meta->mem_ri, 0);
    if (rc < 0) {
      return rc;
    }

    // Sync flash to mem.
    rc = sync_events(meta->flash_ri, meta->mem_ri);
    if (rc < 0) {
      return rc;
    }
  }

  // Log a clear event (to both logs).
  return write_clear_event(pblog);
}

static enum pblog_status pblog_add_event(struct pblog *pblog, pblog_Event *event) {
  struct pblog_metadata *meta = pblog->priv;
  int rc = write_event(pblog, event);

  if (meta->allow_clear_on_add && rc == PBLOG_ERR_NO_SPACE) {
    rc = log_compact(pblog);
    if (rc < 0) {
      PBLOG_ERRF("log full, failed to free space");
      return rc;
    }

    rc = write_event(pblog, event);
  }

  return rc;
}

static enum pblog_status pblog_for_each_event(struct pblog *pblog,
                                              pblog_event_cb callback,
                                              pblog_Event *event,
                                              void *priv) {
  struct pblog_metadata *meta = pblog->priv;
  // Prefer reading from the memory-based log if available.
  struct record_intf *ri = meta->mem_ri ? meta->mem_ri : meta->flash_ri;
  int offset = 0;

  while (1) {
    size_t len = PBLOG_MAX_EVENT_SIZE;
    unsigned char event_buf[PBLOG_MAX_EVENT_SIZE];
    int next_offset = 0;
    int event_valid;

    int rc = ri->read_record(ri, offset, &next_offset, &len, event_buf);
    if (rc < 0 && rc != PBLOG_ERR_CHECKSUM) {
      return rc;
    }
    if (next_offset == 0) {  // end of log?
      break;
    }

    event_valid = rc != PBLOG_ERR_CHECKSUM;
    // Decode the event.
    rc = event_decode(event_buf, len, event);
    if (rc < 0) {
      event_valid = 0;
    }

    // Notify callback.
    if (callback && (*callback)(event_valid, event, priv) != PBLOG_SUCCESS) {
      break;
    }

    offset += next_offset;
  }

  return PBLOG_SUCCESS;
}

static enum pblog_status pblog_for_each_event_internal(struct pblog *pblog,
                                                       pblog_event_cb callback,
                                                       void *priv) {
  enum pblog_status status;
  pblog_Event *event = (pblog_Event*) malloc(sizeof(pblog_Event));
  if (event == NULL)
      return PBLOG_ERR_NO_SPACE;
  event_init(event);
  status = pblog_for_each_event(pblog, callback, event, priv);
  event_free(event);
  free(event);
  return status;
}

static enum pblog_status pblog_clear(struct pblog *pblog) {
  struct pblog_metadata *meta = pblog->priv;
  // Erase the data.
  int rc = meta->flash_ri->clear(meta->flash_ri, 0);
  if (rc < 0) {
    PBLOG_ERRF("pblog: flash clear error\n");
    return rc;
  }
  if (meta->mem_ri) {
    rc = meta->mem_ri->clear(meta->mem_ri, 0);
    if (rc < 0) {
      PBLOG_ERRF("pblog: mem clear error\n");
      return rc;
    }
  }

  // Log a clear event.
  return write_clear_event(pblog);
}

// Synchronizes events between 2 record sources.  Skips corrupt/invalid
// records.
static int sync_events(struct record_intf *source, struct record_intf *dest) {
  int offset = 0;

  while (1) {
    size_t len = PBLOG_MAX_EVENT_SIZE;
    unsigned char event_buf[PBLOG_MAX_EVENT_SIZE];
    int next_offset = 0;
    int rc = source->read_record(source, offset, &next_offset, &len, event_buf);
    if (next_offset == 0) {
      break;
    }

    if (rc >= 0) {
      rc = dest->append(dest, len, event_buf);
      if (rc < 0) {
        PBLOG_ERRF("pblog: failed to sync event to dest\n");
        return rc;
      }
    } else {
      PBLOG_DPRINTF("pblog: skipping corrupt record at offset %d\n", offset);
    }

    offset += next_offset;
  }

  return PBLOG_SUCCESS;
}

static struct record_intf *pblog_init_memlog(void *addr, size_t size,
                                             struct record_intf *flash_ri) {
  struct record_region mem_region;
  struct record_intf *mem_ri;
  int rc;

  pblog_mem_ops.priv = addr;
  mem_region.offset = 0;
  mem_region.size = size;
  mem_ri = (struct record_intf*)malloc(sizeof(struct record_intf));
  record_intf_init(mem_ri, &mem_region, 1, &pblog_mem_ops);

  // Initialize the contents of the mem log with the flash log.
  rc = sync_events(flash_ri, mem_ri);
  if (rc < 0) {
    PBLOG_ERRF("pblog: failed to initialize memlog\n");
  }

  return mem_ri;
}

static enum pblog_status count_events_callback(int valid,
                                               const pblog_Event *event,
                                               void *priv) {
  int *count = priv;
  (void)event;
  if (valid)
    (*count)++;
  return PBLOG_SUCCESS;
}

// Check if this is a newly initialized log due to first time use or corruption.
// If this is the first time write a clear event with the current timestamp.
static int pblog_first_time_init(struct pblog *pblog) {
  int count = 0;
  int rc = pblog_for_each_event_internal(pblog, count_events_callback, &count);
  if (rc < 0) {
    return rc;
  }
  if (count == 0) {
    PBLOG_DPRINTF("pblog first time init\n");
    rc = write_clear_event(pblog);
    if (rc < 0) {
      return rc;
    }
    count = 1;
  }

  return count;
}

int pblog_init(struct pblog *pblog,
               int allow_clear_on_add,
               struct record_intf *flash_ri,
               void *mem_addr, size_t mem_size) {
  struct pblog_metadata *meta = malloc(sizeof(struct pblog_metadata));
  if (meta == NULL) {
    return PBLOG_ERR_NO_SPACE;
  }

  meta->flash_ri = flash_ri;
  meta->allow_clear_on_add = allow_clear_on_add;
  if (mem_addr != NULL) {
    meta->mem_ri = pblog_init_memlog(mem_addr, mem_size, flash_ri);
  } else {
    meta->mem_ri = NULL;
  }

  pblog->priv = meta;

  pblog->add_event = pblog_add_event;
  pblog->for_each_event = pblog_for_each_event;
  pblog->clear = pblog_clear;

  return pblog_first_time_init(pblog);
}

void pblog_free(struct pblog *pblog) {
  struct pblog_metadata *meta = pblog->priv;
  record_intf_free(meta->mem_ri);
  free(meta->mem_ri);
  pblog_mem_ops.priv = NULL;

  free(meta);
}
