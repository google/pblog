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

/* NVRAM basic support */

#include <string.h>
#include <stdlib.h>

#include <pblog/common.h>
#include <pblog/flash.h>
#include <pblog/nvram.h>
#include <pblog/record.h>

#define MAX_NVRAM_ENTRIES 1024
#define MAX_NVRAM_ENTRY_SIZE 1024
static const char kDelimiter = '\0';

static int nvram_addentry(struct nvram *nvram, const char *key,
                          const char *data, size_t data_len) {
  char entry_buf[MAX_NVRAM_ENTRY_SIZE];
  int rc;
  int key_len = strlen(key);
  int entry_len = key_len + sizeof(kDelimiter) + data_len;
  if (entry_len > sizeof(entry_buf)) {
    return -1;
  }

  memcpy(entry_buf, key, key_len);
  entry_buf[key_len] = kDelimiter;
  memcpy(entry_buf + key_len + sizeof(kDelimiter), data, data_len);

  rc = nvram->ri->append(nvram->ri, entry_len, entry_buf);
  return rc < 0 ? rc : 0;
}

static int nvram_parse_entry(const char *entry, size_t len,
                             char **key, char **data) {
  int key_len, data_len;
  for (key_len = 0; key_len < len &&
      memcmp(&entry[key_len], &kDelimiter, sizeof(kDelimiter)) != 0;
      ++key_len) {
  }
  *key = malloc(key_len + sizeof('\0'));
  memcpy(*key, entry, key_len);
  (*key)[key_len] = '\0';

  data_len = len - key_len - sizeof(kDelimiter);
  if (data_len > 0) {
    *data = malloc(data_len + sizeof('\0'));
    memcpy(*data, entry + key_len + sizeof(kDelimiter), data_len);
    (*data)[data_len] = '\0';
  } else {
    *data = NULL;
  }
  return data_len;
}

static const struct nvram_entry *find_key(const struct nvram_entry *entries,
                                          const char *key) {
  for (; entries->key != NULL; ++entries) {
    if (strcmp(entries->key, key) == 0) {
      return entries;
    }
  }
  return NULL;
}

void nvram_entry_free(struct nvram_entry *entry) {
  free(entry->key);
  free(entry->data);
}

static int nvram_enumerate(struct nvram *nvram, struct nvram_entry **entries) {
  int num_entries = 0;
  int offset = 0;
  int array_size = 0;

  *entries = NULL;
  do {
    char entry_buf[MAX_NVRAM_ENTRY_SIZE];
    size_t len = sizeof(entry_buf);
    int next_offset;
    int rc;
    int data_len;

    rc = nvram->ri->read_record(nvram->ri, offset, &next_offset, &len,
                                entry_buf);
    if (rc < 0) {
      return rc;
    }
    if (next_offset == 0) {
      break;
    }
    offset += next_offset;
    num_entries++;
    if (array_size < num_entries) {
      array_size = num_entries * 2 + 1;  // grow exponentially for efficiency
      *entries = realloc(*entries, array_size * sizeof(struct nvram_entry));
    }
    data_len = nvram_parse_entry(entry_buf,
                                 len,
                                 &(*entries)[num_entries - 1].key,
                                 &(*entries)[num_entries - 1].data);
    (*entries)[num_entries - 1].data_len = data_len;
  } while (1);

  *entries = realloc(*entries, (num_entries + 1) * sizeof(struct nvram_entry));
  (*entries)[num_entries].key = NULL;
  (*entries)[num_entries].data = NULL;
  (*entries)[num_entries].data_len = 0;

  return 0;
}

void nvram_list_free(struct nvram_entry **entries) {
  struct nvram_entry *entry;
  for (entry = *entries; entry && entry->key != NULL; entry++) {
    nvram_entry_free(entry);
  }
  free(*entries);
}

const struct nvram_entry *nvram_list_find(const struct nvram_entry *entries,
                                          const char *key) {
  const struct nvram_entry *entry = entries;
  const struct nvram_entry *lastentry = NULL;

  while ((entry = find_key(entry, key)) != NULL) {
    lastentry = entry;
    entry++;
  }

  return lastentry;
}

static int nvram_list_count(struct nvram_entry *entries) {
  int num_entries = 0;
  for (; entries->key != NULL; ++entries, ++num_entries) {
  }
  return num_entries;
}

static void nvram_list_compact(struct nvram_entry **entries,
                               const char *new_key) {
  int num_entries = nvram_list_count(*entries);
  int index;

  for (index = 0; index < num_entries;) {
    struct nvram_entry *entry = &(*entries)[index];
    int empty_data = entry->data == NULL;
    int newer_key_exists = find_key(entry + 1, entry->key) != NULL;
    int matches_new_key = new_key && strcmp(entry->key, new_key) == 0;
    // If any of these are true then this entry is old/overwritten,
    // remove it from the array by the simple "copy-back" algorithm.
    if (empty_data || newer_key_exists || matches_new_key) {
      nvram_entry_free(entry);
      memmove(entry, entry + 1,
              (num_entries - index) * sizeof(struct nvram_entry));
      num_entries--;
    } else {
      index++;
    }
  }
}

static int nvram_compact(struct nvram *nvram, const char *new_key) {
  // Read all of the NVRAM entries into memory.
  struct nvram_entry *entry;
  struct nvram_entry *entries = NULL;
  int num_new = 0, num_old = 0;
  int rc = nvram_enumerate(nvram, &entries);
  if (rc < 0) {
    return rc;
  }

  num_old = nvram_list_count(entries);
  if (num_old < 2) {
    rc = 0;
    goto out;
  }

  // Compact in memory
  nvram_list_compact(&entries, new_key);
  num_new = nvram_list_count(entries);
  if (num_new >= num_old) {
    PBLOG_ERRF("could not free any entries");
    rc = -1;
    goto out;
  }

  // Clear the NVRAM storage.
  rc = nvram->ri->clear(nvram->ri, 0);
  if (rc < 0) {
    goto out;
  }

  // Write out the more compact version.
  for (entry = entries; entry->key != NULL; ++entry) {
    rc = nvram_addentry(nvram, entry->key, entry->data, entry->data_len);
    if (rc < 0) {
      PBLOG_ERRF("failure adding key %s\n", entry->key);
    }
  }

 out:
  // Free our allocated entries.
  nvram_list_free(&entries);

  return (rc >= 0) ? num_old - num_new : rc;
}

static int nvram_set(struct nvram *nvram, const char *key, const char *data,
                     size_t data_len) {
  int length = strlen(key) + data_len + sizeof(kDelimiter);
  if (length * 2 > nvram->ri->get_free_space(nvram->ri)) {
    // Need to free up some room.
    int num_freed = nvram_compact(nvram, key);
    PBLOG_DPRINTF("freed %d NVRAM entries\n", num_freed);
    if (num_freed < 0) {
      return num_freed;
    }
  }

  return nvram_addentry(nvram, key, data, data_len);
}

static int nvram_lookup(struct nvram *nvram, const char *key, char *data,
                        size_t max_data_len) {
  int data_len = -1;
  int offset = 0;
  do {
    char entry_buf[MAX_NVRAM_ENTRY_SIZE];
    size_t entry_len = sizeof(entry_buf);
    struct nvram_entry entry;
    int next_offset;
    int rc = nvram->ri->read_record(nvram->ri, offset, &next_offset,
                                    &entry_len, entry_buf);
    if (rc < 0 || next_offset == 0) {
      break;
    }
    offset += next_offset;

    data_len = nvram_parse_entry(entry_buf, entry_len, &entry.key, &entry.data);
    if (data_len > 0 &&
        data_len <= (max_data_len - sizeof('\0')) &&
        strcmp(key, entry.key) == 0) {
      memcpy(data, entry.data, data_len);
    }
    nvram_entry_free(&entry);
    /* Keep going, we may find a newer version of this key */
  } while (1);

  return data_len;
}

static int nvram_unset(struct nvram *nvram, const char *key) {
  return nvram->set(nvram, key, "", 0);
}

// Read all of the valid NVRAM entries into memory.
static int nvram_list(struct nvram *nvram, struct nvram_entry **entries) {
  int rc = nvram_enumerate(nvram, entries);
  if (rc < 0) {
    return rc;
  }

  nvram_list_compact(entries, NULL);
  return 0;
}

// Clear and reinitialize the entire NVRAM storage.
static int nvram_clear(struct nvram *nvram) {
  int rc = nvram->ri->clear(nvram->ri, 0);
  if (rc < 0) {
    return rc;
  }
  return 0;
}

void pblog_nvram_init(struct nvram *nvram, struct record_intf *ri) {
  nvram->ri = ri;

  nvram->lookup = nvram_lookup;
  nvram->set = nvram_set;
  nvram->unset = nvram_unset;
  nvram->list = nvram_list;
  nvram->clear = nvram_clear;
}

void pblog_nvram_free(struct nvram *nvram) {
  record_intf_free(nvram->ri);
}

#ifdef NVRAM_CMDLINE_APP
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <pblog/file.h>

int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(stderr, "usage: %s <file> [key] [data]\n", argv[0]);
    return 1;
  }

  const char *filename = argv[1];

  char *key = NULL;
  if (argc >= 3) {
    key = argv[2];
  }

  char *data = NULL;
  if (argc >= 4) {
    data = argv[3];
  }

  int rc;

  pblog_file_ops.priv = (void*)filename;

  struct record_region regions[1];
  regions[0].offset = 0;
  regions[0].size = 0xff;

  struct record_intf ri;
  record_intf_init(&ri, regions, 1, &pblog_file_ops);

  struct nvram nvram;
  nvram_init(&nvram, &ri);

  if (!key) {
    struct nvram_entry *entries;
    rc = nvram.list(&nvram, &entries);
    if (rc < 0) {
      return rc;
    }
    struct nvram_entry *entry = entries;
    for (; entry->key != NULL; entry++) {
      fprintf(stdout, "%s=%s\n", entry->key, entry->data);
    }
    nvram_list_free(&entries);
    return 0;
  } else if (!data) {
    data = malloc(MAX_NVRAM_ENTRY_SIZE);
    rc = nvram.lookup(&nvram, key, data, MAX_NVRAM_ENTRY_SIZE);
    if (rc < 0) {
      return rc;
    }
    fprintf(stdout, "%s=%s\n", key, data);
    free(data);
    return rc;
  } else {
    rc = nvram.set(&nvram, key, data, strlen(data));
    return rc;
  }

  return 0;
}
#endif
