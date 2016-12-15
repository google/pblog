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

#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <pblog/event.h>
#include <pblog/file.h>
#include <pblog/pblog.h>
#include <pblog/record.h>

#include "common.hh"

namespace {

using std::string;
using std::vector;

class PblogFileTest : public ::testing::Test {
 public:
  PblogFileTest() {
    filename_ = "/tmp/pblog.tst";
    flash_ri_ = NULL;
    mem_log_ = NULL;
    pblog_ = NULL;
    events = new vector<pblog_Event *>;
  }

  virtual ~PblogFileTest() {
    clear_state();
    delete events;

    unlink(filename_.c_str());
  }

  void init_2regions(int offset0, int size0, int offset1, int size1) {
    struct record_region file_regions[2];
    file_regions[0].offset = offset0;
    file_regions[0].size = size0;
    file_regions[0].used_size = 0;
    file_regions[1].offset = offset1;
    file_regions[1].size = size1;
    file_regions[1].used_size = 0;

    flash_ri_ = reinterpret_cast<struct record_intf *>(
        malloc(sizeof(struct record_intf)));
    pblog_file_ops.priv = (void *)filename_.c_str();
    record_intf_init(flash_ri_, file_regions, 2, &pblog_file_ops);

    mem_log_ = malloc(size0 + size1);

    pblog_ = static_cast<struct pblog *>(malloc(sizeof(struct pblog)));
    pblog_->get_current_bootnum = NULL;
    pblog_->get_time_now = NULL;
    pblog_init(pblog_, 0, flash_ri_, mem_log_, size0 + size1);
  }

  void clear_state() {
    for (size_t i = 0; i < events->size(); ++i) {
      delete events->at(i);
    }
    events->clear();

    pblog_free(pblog_);
    free(pblog_);
    pblog_ = NULL;

    record_intf_free(flash_ri_);
    free(flash_ri_);
    flash_ri_ = NULL;

    free(mem_log_);
  }

  record_intf *flash_ri_;
  void *mem_log_;
  pblog *pblog_;

  string filename_;
  static vector<pblog_Event *> *events;
};

vector<pblog_Event *> *PblogFileTest::events;

pblog_status collect_events_cb(int valid, const pblog_Event *event,
                               void *priv) {
  pblog_Event *new_event = new pblog_Event;
  memcpy(new_event, event, sizeof(*event));
  PblogFileTest::events->push_back(new_event);
  return PBLOG_SUCCESS;
}

TEST_F(PblogFileTest, TotallyEmptyLog) {
  init_2regions(0, 0xff, 0x100, 0xff);
  pblog_Event event;

  EXPECT_EQ(0, pblog_->for_each_event(pblog_, collect_events_cb, &event, NULL));
  EXPECT_EQ(1, events->size());

  // Should log a clear event.
  EXPECT_EQ(pblog_TYPE_LOG_CLEARED, events->at(0)->type);
}

TEST_F(PblogFileTest, LogClearedSuccess) {
  init_2regions(0, 0xff, 0x100, 0xff);
  pblog_Event event;

  EXPECT_EQ(0, pblog_->clear(pblog_));

  // Should be a single clear event.
  EXPECT_EQ(0, pblog_->for_each_event(pblog_, collect_events_cb, &event, NULL));
  ASSERT_EQ(1, events->size());
  EXPECT_EQ(pblog_TYPE_LOG_CLEARED, events->at(0)->type);
}

TEST_F(PblogFileTest, ClearNotEnoughRoom) {
  // Only allow 1 byte per region.
  init_2regions(0, 1, 0x100, 1);

  // Clear should return failure status.
  EXPECT_NE(0, pblog_->clear(pblog_));
}

TEST_F(PblogFileTest, LogAFewEvents) {
  init_2regions(0, 0xff, 0x100, 0xff);
  pblog_Event event;

  size_t num_events = 10;
  for (size_t i = 0; i < num_events; ++i) {
    pblog_Event event;
    event_init(&event);
    event.type = pblog_TYPE_BOOT_UP;
    EXPECT_EQ(0, pblog_->add_event(pblog_, &event));
    event_free(&event);
  }

  EXPECT_EQ(0, pblog_->for_each_event(pblog_, collect_events_cb, &event, NULL));
  ASSERT_EQ(1 + num_events, events->size());
}

TEST_F(PblogFileTest, LogSecondRegion) {
  // First region is small.
  init_2regions(0, 30, 0x100, 0xff);
  pblog_Event event;

  size_t num_events = 10;
  for (size_t i = 0; i < num_events; ++i) {
    pblog_Event event;
    event_init(&event);
    event.type = pblog_TYPE_BOOT_UP;
    EXPECT_EQ(0, pblog_->add_event(pblog_, &event));
    event_free(&event);
  }

  EXPECT_EQ(0, pblog_->for_each_event(pblog_, collect_events_cb, &event, NULL));
  ASSERT_EQ(1 + num_events, events->size());
}

TEST_F(PblogFileTest, LogFull) {
  // Both regions are small.
  init_2regions(0, 30, 0x100, 30);
  pblog_Event event;

  size_t num_events = 8;
  for (size_t i = 0; i < num_events; ++i) {
    pblog_Event event;
    event_init(&event);
    event.type = pblog_TYPE_BOOT_UP;
    if (i == num_events - 1) {
      EXPECT_NE(0, pblog_->add_event(pblog_, &event));
    } else {
      EXPECT_EQ(0, pblog_->add_event(pblog_, &event));
    }
    event_free(&event);
  }

  EXPECT_EQ(0, pblog_->for_each_event(pblog_, collect_events_cb, &event, NULL));
  ASSERT_EQ(1 + num_events - 1, events->size());
}

TEST_F(PblogFileTest, LogPersists) {
  init_2regions(0, 0xff, 0x100, 0xff);
  pblog_Event event;

  size_t num_events = 4;
  for (size_t i = 0; i < num_events; ++i) {
    pblog_Event event;
    event_init(&event);
    event.type = pblog_TYPE_BOOT_UP;
    EXPECT_EQ(0, pblog_->add_event(pblog_, &event));
    event_free(&event);
  }

  EXPECT_EQ(0, pblog_->for_each_event(pblog_, collect_events_cb, &event, NULL));
  ASSERT_EQ(1 + num_events, events->size());

  clear_state();
  init_2regions(0, 0xff, 0x100, 0xff);

  EXPECT_EQ(0, pblog_->for_each_event(pblog_, collect_events_cb, &event, NULL));
  ASSERT_EQ(1 + num_events, events->size());

  clear_state();
  // Switch the order of the region offsets, should not make a difference.
  init_2regions(0x100, 0xff, 0, 0xff);

  EXPECT_EQ(0, pblog_->for_each_event(pblog_, collect_events_cb, &event, NULL));
  ASSERT_EQ(1 + num_events, events->size());

  // Clear the log.
  EXPECT_EQ(0, pblog_->clear(pblog_));

  clear_state();
  init_2regions(0, 0xff, 0x100, 0xff);

  EXPECT_EQ(0, pblog_->for_each_event(pblog_, collect_events_cb, &event, NULL));
  ASSERT_EQ(1, events->size());
}

}  // namespace
