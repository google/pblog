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

#include <limits.h>
#include <string.h>

#include <nanopb/pb.h>
#include <nanopb/pb_decode.h>
#include <nanopb/pb_encode.h>
#include <pblog/common.h>
#include <pblog/event.h>
#include <pblog/pblog.pb.h>

static bool string_encoder(pb_ostream_t *stream, const pb_field_t *field,
                           void *const *arg) {
  const char *str = (const char *)*arg;
  if (str == NULL) {
    return true;
  }
  if (!pb_encode_tag_for_field(stream, field)) {
    return false;
  }
  return pb_encode_string(stream, (uint8_t *)str, strlen(str));
}

static bool string_decoder(pb_istream_t *stream, const pb_field_t *field,
                           void **arg) {
  int strsize = stream->bytes_left;
  char *str = (char *)malloc(strsize + 1);
  if (str == NULL) {
    return false;
  }

  pb_read(stream, (uint8_t *)str, strsize);
  str[strsize] = '\0';
  if (*arg) free(*arg);
  *arg = str;
  return true;
}

int event_encode(const pblog_Event *event, void *buf, size_t len) {
  pb_ostream_t stream = pb_ostream_from_buffer((uint8_t *)buf, len);

  /* Now encode it and check if we succeeded. */
  if (pb_encode(&stream, pblog_Event_fields, event)) {
    return stream.bytes_written;
  }

  PBLOG_ERRF("event encode error: %s\n", PB_GET_ERROR(&stream));
  return PBLOG_ERR_INVALID;
}

int event_decode(const void *buf, size_t len, pblog_Event *event) {
  pb_istream_t stream = pb_istream_from_buffer((uint8_t *)buf, len);
  int i = 0;

  for (i = 0; i < pb_arraysize(pblog_Event, data); ++i) {
    event->data[i].key.funcs.decode = string_decoder;
    event->data[i].value.funcs.decode = string_decoder;
  }
  if (pb_decode(&stream, pblog_Event_fields, event)) {
    return 0;
  }

  PBLOG_ERRF("event decode error: %s\n", PB_GET_ERROR(&stream));
  return PBLOG_ERR_INVALID;
}

static bool nul_write_callback(pb_ostream_t *stream, const uint8_t *buf,
                               size_t count) {
  (void)stream;
  (void)buf;
  (void)count;
  return true;
}

int event_size(const pblog_Event *event) {
  pb_ostream_t stream;
  stream.callback = &nul_write_callback;
  stream.max_size = INT_MAX;

  if (pb_encode(&stream, pblog_Event_fields, event)) {
    return stream.bytes_written;
  }

  return PBLOG_ERR_INVALID;
}

void event_init(pblog_Event *event) { memset(event, 0, sizeof(*event)); }

void event_free(pblog_Event *event) {
  int i = 0;
  for (i = 0; i < event->data_count; ++i) {
    free(event->data[i].key.arg);
    event->data[i].key.arg = NULL;
    free(event->data[i].value.arg);
    event->data[i].value.arg = NULL;
  }
}

void event_add_kv_data(pblog_Event *entry, const char *key, const char *value) {
  if (entry->data_count >= pb_arraysize(pblog_Event, data)) {
    return;
  }

  entry->data[entry->data_count].key.arg = strdup(key);
  entry->data[entry->data_count].key.funcs.encode = string_encoder;
  entry->data[entry->data_count].value.arg = strdup(value);
  entry->data[entry->data_count].value.funcs.encode = string_encoder;

  entry->data_count++;
}
