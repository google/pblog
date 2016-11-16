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

#include "file.h"
#include "common.h"
#include "record.h"

#include <string>
#include "base/stringprintf.h"
#include "file/base/fileutils.h"
#include "file/base/helpers.h"
#include "strings/escaping.h"
#include "util/gtl/container_literal.h"
#include "testing/base/public/gunit.h"

namespace {

using ::util::gtl::Container;

class RecordFileTest : public ::testing::Test {
 public:
  RecordFileTest() {
    filename_ = FLAGS_test_tmpdir + "/record.tst";
    ri_ = NULL;
  }

  virtual ~RecordFileTest() {
    ClearState();

    unlink(filename_.c_str());
  }

  void InitRegions(const vector<pair<int, int> > &regions) {
    struct record_region *region_structs =
        new struct record_region[regions.size()];
    for (int i = 0; i < regions.size(); ++i) {
      memset(&region_structs[i], 0, sizeof(region_structs[i]));
      region_structs[i].offset = regions[i].first;
      region_structs[i].size = regions[i].second;
    }

    ri_ = new struct record_intf;
    pblog_file_ops.priv = (void*)filename_.c_str();
    ASSERT_EQ(0, record_intf_init(ri_, region_structs, regions.size(),
                                  &pblog_file_ops));
    delete[] region_structs;
  }

  void ClearState() {
    record_intf_free(ri_);
    delete ri_;
    ri_ = NULL;
  }

  int NumValidRecords() {
    int num_records = 0;
    int offset = 0;
    while (true) {
      int next_offset = 0;
      size_t len = 4096;
      string data(len, '\0');
      int rc = ri_->read_record(ri_, offset, &next_offset, &len, &data[0]);
      if (len == 0 || next_offset == 0) {
        break;
      }
      offset += next_offset;
      num_records += (rc < 0) ? 0 : 1;
    }

    return num_records;
  }

  int GetRecord(int i, string *record_data) {
    int num_records = 0;
    size_t len = 4096;
    string data(len, '\0');
    int offset = 0;
    int rc = 0;
    while (num_records <= i) {
      int next_offset = 0;
      rc = ri_->read_record(ri_, offset, &next_offset, &len, &data[0]);
      if (len == 0 || next_offset == 0) {
        return -1;
      }
      offset += next_offset;
      num_records++;
    }

    if (record_data) {
      *record_data = data.substr(0, len);
    }
    return rc;
  }

  int FillWithRecords() {
    string expected_data;
    int num_written = 0, rc = 0;
    while (1) {
      expected_data = StringPrintf("%08x", num_written);
      rc = ri_->append(ri_, expected_data.size(), &expected_data[0]);
      EXPECT_TRUE(rc > 0 || rc == PBLOG_ERR_NO_SPACE);
      if (rc == PBLOG_ERR_NO_SPACE) {
        break;
      }
      num_written++;
    }
    return num_written;
  }

  record_intf *ri_;

  string filename_;
};

TEST_F(RecordFileTest, FirstTimeInit) {
  InitRegions(Container(make_pair(0, 0xff), make_pair(0x100, 0xff)));

  EXPECT_LT(ri_->get_free_space(ri_), 0xff + 0xff);
  EXPECT_GT(ri_->get_free_space(ri_), 0xff);

  EXPECT_EQ(0, NumValidRecords());
  EXPECT_NE(0, GetRecord(0, NULL));
}

TEST_F(RecordFileTest, InitWithGarbage) {
  // Fill the file with garbage.
  string garbage(4096, 0);
  for (int i = 0; i < garbage.size(); ++i) {
    garbage[i] = i;
  }
  CHECK_OK(file::SetContents(filename_, garbage, file::Defaults()));

  InitRegions(Container(make_pair(0, 0xff), make_pair(0x100, 0xff)));

  EXPECT_LT(ri_->get_free_space(ri_), 0xff + 0xff);
  EXPECT_GT(ri_->get_free_space(ri_), 0xff);

  EXPECT_EQ(0, NumValidRecords());
  EXPECT_NE(0, GetRecord(0, NULL));
}

TEST_F(RecordFileTest, AddSingleRecord) {
  InitRegions(Container(make_pair(0, 0xff), make_pair(0x100, 0xff)));

  const string expected_data("asdfjkl1111000");
  EXPECT_GE(ri_->append(ri_, expected_data.size(), &expected_data[0]),
            expected_data.size());

  EXPECT_EQ(1, NumValidRecords());
  string data;
  EXPECT_EQ(0, GetRecord(0, &data));
  EXPECT_EQ(expected_data, data);
}

TEST_F(RecordFileTest, ReadRecordBuffertoSmall) {
  InitRegions(Container(make_pair(0, 0xff), make_pair(0x100, 0xff)));

  const string expected_data("asdfjkl1111000");
  EXPECT_GE(ri_->append(ri_, expected_data.size(), &expected_data[0]),
            expected_data.size());

  EXPECT_EQ(1, NumValidRecords());

  int next_offset;
  string data(expected_data.size() - 1, 0);
  size_t len = data.size();
  EXPECT_EQ(PBLOG_ERR_NO_SPACE,
            ri_->read_record(ri_, 0, &next_offset, &len, &data));
  EXPECT_EQ(expected_data.size(), len);
  EXPECT_GT(next_offset, 0);
}

TEST_F(RecordFileTest, FillWithRecords) {
  InitRegions(Container(make_pair(0, 0xff), make_pair(0x100, 0xff)));

  int num_written = FillWithRecords();
  ASSERT_GT(num_written, 0);

  EXPECT_LT(ri_->get_free_space(ri_), 8);

  EXPECT_EQ(num_written, NumValidRecords());
  for (int i = 0; i < num_written; ++i) {
    string data;
    EXPECT_EQ(0, GetRecord(i, &data));
    EXPECT_EQ(StringPrintf("%08x", i), data);
  }
}

TEST_F(RecordFileTest, ClearAllRecords) {
  InitRegions(Container(make_pair(0, 0xff), make_pair(0x100, 0xff)));

  int num_written = FillWithRecords();
  ASSERT_GT(num_written, 0);
  EXPECT_EQ(num_written, NumValidRecords());
  EXPECT_LT(ri_->get_free_space(ri_), 8);

  EXPECT_EQ(0xff + 0xff, ri_->clear(ri_, 0));

  EXPECT_EQ(0, NumValidRecords());
  EXPECT_NE(0, GetRecord(0, NULL));
  EXPECT_GT(ri_->get_free_space(ri_), 0xff);
}

TEST_F(RecordFileTest, ClearOneRegion) {
  InitRegions(Container(make_pair(0, 0x7f), make_pair(0x100, 0xff)));

  int num_written = FillWithRecords();
  ASSERT_GT(num_written, 0);
  EXPECT_EQ(num_written, NumValidRecords());
  EXPECT_LT(ri_->get_free_space(ri_), 8);

  EXPECT_EQ(0x7f, ri_->clear(ri_, 1));

  int num_records_after_clear = NumValidRecords();
  int num_cleared = num_written - num_records_after_clear;
  EXPECT_GT(num_cleared, 0);

  for (int i = 0; i < num_records_after_clear; ++i) {
    string data;
    EXPECT_EQ(0, GetRecord(i, &data));
    EXPECT_EQ(StringPrintf("%08x", i + num_cleared), data);
  }
}

TEST_F(RecordFileTest, RecordsPersist) {
  InitRegions(Container(make_pair(0, 0xff), make_pair(0x100, 0xff)));

  const string expected_data("asdfjkl1111000");
  EXPECT_GE(ri_->append(ri_, expected_data.size(), &expected_data[0]),
            expected_data.size());
  EXPECT_GE(ri_->append(ri_, expected_data.size(), &expected_data[0]),
            expected_data.size());

  EXPECT_EQ(2, NumValidRecords());

  ClearState();
  InitRegions(Container(make_pair(0, 0xff), make_pair(0x100, 0xff)));

  EXPECT_EQ(2, NumValidRecords());
  string data;
  EXPECT_EQ(0, GetRecord(0, &data));
  EXPECT_EQ(expected_data, data);
  EXPECT_EQ(0, GetRecord(1, &data));
  EXPECT_EQ(expected_data, data);
}

TEST_F(RecordFileTest, CorruptRecordData) {
  InitRegions(Container(make_pair(0, 0xff), make_pair(0x100, 0xff)));

  const string expected_data("asdfjkl1111000");
  EXPECT_GE(ri_->append(ri_, expected_data.size(), &expected_data[0]),
            expected_data.size());
  EXPECT_GE(ri_->append(ri_, expected_data.size(), &expected_data[0]),
            expected_data.size());

  EXPECT_EQ(2, NumValidRecords());

  // Corrupt the 1st byte of 1st record data with a NUL byte
  int offset = sizeof(record_header) + sizeof(region_header);
  unsigned char val = 0;
  EXPECT_EQ(sizeof(val), pblog_file_ops.write(&pblog_file_ops, offset,
                                              sizeof(val), &val));

  EXPECT_EQ(1, NumValidRecords());
  string data;
  // Should return checksum error but still read the data.
  EXPECT_EQ(PBLOG_ERR_CHECKSUM, GetRecord(0, &data));
  EXPECT_EQ(expected_data.size(), data.size());
  EXPECT_NE(expected_data, data);

  EXPECT_EQ(0, GetRecord(1, &data));
  EXPECT_EQ(expected_data, data);

  // Make sure the corrupt record does not cause problems on re-init.
  ClearState();
  InitRegions(Container(make_pair(0, 0xff), make_pair(0x100, 0xff)));

  EXPECT_EQ(1, NumValidRecords());
  // Should return checksum error but still read the data.
  EXPECT_EQ(PBLOG_ERR_CHECKSUM, GetRecord(0, &data));
  EXPECT_EQ(expected_data.size(), data.size());
  EXPECT_NE(expected_data, data);

  EXPECT_EQ(0, GetRecord(1, &data));
  EXPECT_EQ(expected_data, data);
}

TEST_F(RecordFileTest, CorruptRecordLength) {
  InitRegions(Container(make_pair(0, 0xff), make_pair(0x100, 0xff)));

  const string expected_data("asdfjkl1111000");
  EXPECT_GE(ri_->append(ri_, expected_data.size(), &expected_data[0]),
            expected_data.size());
  EXPECT_GE(ri_->append(ri_, expected_data.size(), &expected_data[0]),
            expected_data.size());

  EXPECT_EQ(2, NumValidRecords());

  // Try many possible length corruptions.
  for (int i = 0; i < 0x600; ++i) {
    if (i == expected_data.size() + sizeof(record_header)) {
      continue;
    }
    int offset = sizeof(region_header);
    uint8 val[] = { static_cast<uint8>((i >> 8) & 0xff),
        static_cast<uint8>(i & 0xff) };
    EXPECT_EQ(sizeof(val), pblog_file_ops.write(&pblog_file_ops, offset,
                                                sizeof(val), &val));

    ASSERT_EQ(0, NumValidRecords()) << "for value " << i;
    string data;
    EXPECT_NE(0, GetRecord(0, &data));

    ClearState();
    InitRegions(Container(make_pair(0, 0xff), make_pair(0x100, 0xff)));

    ASSERT_EQ(0, NumValidRecords()) << "for value " << i;
    EXPECT_NE(0, GetRecord(0, &data));
  }
}

TEST_F(RecordFileTest, BigLog) {
  InitRegions(Container(make_pair(0, 4096),
                        make_pair(4096, 4096),
                        make_pair(8192, 4096),
                        make_pair(12288, 4096)));

  // We should be able to write atleast 1000 records.
  int num_written = FillWithRecords();
  ASSERT_GT(num_written, 1000);
  EXPECT_EQ(num_written, NumValidRecords());

  // Clear 1 region.
  EXPECT_EQ(4096, ri_->clear(ri_, 1));

  // Should be able to write atleast 100 new records.
  int new_written = FillWithRecords();
  ASSERT_GT(new_written, 100);
  EXPECT_EQ(num_written, NumValidRecords());
}

}  // namespace
