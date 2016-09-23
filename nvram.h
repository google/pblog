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

/* Simple NVRAM interface */

#ifndef _PBLOG_NVRAM_H_
#define _PBLOG_NVRAM_H_

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

struct record_intf;

struct nvram_entry {
  char *key;  // must be a C style nul-terminated string
  char *data;  // arbitrary bytes not necessarily nul-terminated
  size_t data_len;
};

typedef struct nvram {
  /* Lookup data based on key.
   * Args:
   *   key: key name
   *   data: output buffer to fill
   *   max_data_len: length of output buffer
   * Returns: number of data bytes read on success, <0 on error
   */
  int (*lookup)(struct nvram *nvram, const char *key, char *data,
                size_t max_data_len);

  /* Set a key value.
   * Args:
   *   key: key name
   *   data: data to associate with key
   *   data_len: size of data
   * Returns: 0 on success
   */
  int (*set)(struct nvram *nvram, const char *key, const char *data,
             size_t data_len);

  /* Unsets the value for a key so future lookups will not return it.
   * Args:
   *   key: key name
   * Returns: 0 on success
   */
  int (*unset)(struct nvram *nvram, const char *key);

  /* Lists all entries.
   * Unset entries do not appear in the list, newer entries override older.
   * Args:
   *   entries: pointer to returned list, must be free'd with nvram_list_free()
   * Returns: 0 on success
   */
  int (*list)(struct nvram *nvram, struct nvram_entry **entries);

  /* Clear all entries
   * Returns: 0 on success
   */
  int (*clear)(struct nvram *nvram);

  struct record_intf *ri;
} nvram;

// Deallocates a single nvram_entry object.
void nvram_entry_free(struct nvram_entry *entry);

// Deallocates an array of nvram_entrys as returned from nvram.list().
void nvram_list_free(struct nvram_entry **entries);

// Finds a particular key in a list of entries and returns a pointer to it.
// Returns NULL if the key is not found.
const struct nvram_entry *nvram_list_find(
  const struct nvram_entry *entries,
  const char *key);

void pblog_nvram_init(struct nvram *nvram, struct record_intf *ri);
void pblog_nvram_free(struct nvram *nvram);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif  /* _PBLOG_NVRAM_H_ */
