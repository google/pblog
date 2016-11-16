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

/* Protobuf eventlog interface . */

#ifndef _PBLOG_PBLOG_H_
#define _PBLOG_PBLOG_H_

#include <stdlib.h>

#include <pblog/common.h>
#include <pblog/event.h>
#include <pblog/pblog.pb.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PBLOG_MAX_EVENT_SIZE 4096

struct record_intf;

/* Args:
 *   valid: 1 if event is considered valid, 0 otherwise
 *   event: decoded event
 *   priv: user data pointer
 */
typedef enum pblog_status (*pblog_event_cb)(int valid, const pblog_Event *event,
    void *priv);

typedef struct pblog {
  /* Adds a single event to the log.  event may be modified to add timestamp
   * and/or bootnum values.
   */
  enum pblog_status (*add_event)(struct pblog *pblog, pblog_Event *event);

  /* Calls provided callback for every event in the log.
   * Args:
   *   callback: will be called in order of oldest to most recent entry.
   *   event: event struct to use for unserializing each event, must be non-NULL
   *   priv: opaque pointer that is passed to callback
   */
  enum pblog_status (*for_each_event)(struct pblog *pblog,
                                      pblog_event_cb callback,
                                      pblog_Event *event,
                                      void *priv);

  /* Clears the entire log. */
  enum pblog_status (*clear)(struct pblog *pblog);

  /* Optional functions that can be provided to fill in bootnum/timestamp for
   * events that do not have it.
   */
  uint32_t (*get_current_bootnum)(struct pblog *pblog);
  uint32_t (*get_time_now)(struct pblog *pblog);

  void *priv;
} pblog;

/* Initialize the log.
 * Args:
 *   allow_clear_on_add: if the log should allow reclaiming space if full
 *     during an add operation.  The alternative is to return PBLOG_ERR_NO_SPACE
 *   flash_ri: the record interface to use to store records on flash
 *   mem_addr: memory address to use for in-memory copy (or NULL for none)
 *   mem_size: size of in-memory buffer, should be the same as the total region
 *     size
 * Returns:
 *   number of events found in the log on success, <0 on failure
 */
int pblog_init(struct pblog *pblog,
               int allow_clear_on_add,
               struct record_intf *flash_ri,
               void *mem_addr,
               size_t mem_size);
void pblog_free(struct pblog *pblog);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif  /* _PBLOG_PBLOG_H_ */
