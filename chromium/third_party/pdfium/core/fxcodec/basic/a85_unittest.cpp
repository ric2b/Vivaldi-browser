// Copyright 2016 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <array>
#include <iterator>
#include <limits>
#include <memory>

#include "core/fxcodec/basic/basicmodule.h"
#include "core/fxcrt/data_vector.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::ElementsAre;

TEST(fxcodec, A85EmptyInput) {
  EXPECT_TRUE(BasicModule::A85Encode({}).empty());
}

// No leftover bytes, just translate 2 sets of symbols.
TEST(fxcodec, A85Basic) {
  // Make sure really big values don't break.
  const uint8_t src_buf[] = {1, 2, 3, 4, 255, 255, 255, 255};
  DataVector<uint8_t> dest_buf = BasicModule::A85Encode(src_buf);

  // Should have 5 chars for each set of 4 and 2 terminators.
  EXPECT_THAT(dest_buf,
              ElementsAre(33, 60, 78, 63, 43, 115, 56, 87, 45, 33, 126, 62));
}

// Leftover bytes.
TEST(fxcodec, A85LeftoverBytes) {
  {
    // 1 Leftover Byte:
    const uint8_t src_buf_1leftover[] = {1, 2, 3, 4, 255};
    DataVector<uint8_t> dest_buf = BasicModule::A85Encode(src_buf_1leftover);

    // 5 chars for first symbol + 2 + 2 terminators.
    EXPECT_THAT(dest_buf, ElementsAre(33, 60, 78, 63, 43, 114, 114, 126, 62));
  }
  {
    // 2 Leftover bytes:
    const uint8_t src_buf_2leftover[] = {1, 2, 3, 4, 255, 254};
    DataVector<uint8_t> dest_buf = BasicModule::A85Encode(src_buf_2leftover);
    // 5 chars for first symbol + 3 + 2 terminators.
    EXPECT_THAT(dest_buf,
                ElementsAre(33, 60, 78, 63, 43, 115, 56, 68, 126, 62));
  }
  {
    // 3 Leftover bytes:
    const uint8_t src_buf_3leftover[] = {1, 2, 3, 4, 255, 254, 253};
    DataVector<uint8_t> dest_buf = BasicModule::A85Encode(src_buf_3leftover);
    // 5 chars for first symbol + 4 + 2 terminators.
    EXPECT_THAT(dest_buf,
                ElementsAre(33, 60, 78, 63, 43, 115, 56, 77, 114, 126, 62));
  }
}

// Test all zeros comes through as "z".
TEST(fxcodec, A85Zeros) {
  {
    // Make sure really big values don't break.
    const uint8_t src_buf[] = {1, 2, 3, 4, 0, 0, 0, 0};
    DataVector<uint8_t> dest_buf = BasicModule::A85Encode(src_buf);

    // Should have 5 chars for first set of 4 + 1 for z + 2 terminators.
    EXPECT_THAT(dest_buf, ElementsAre(33, 60, 78, 63, 43, 122, 126, 62));
  }
  {
    // Should also work if it is at the start:
    const uint8_t src_buf_2[] = {0, 0, 0, 0, 1, 2, 3, 4};
    DataVector<uint8_t> dest_buf = BasicModule::A85Encode(src_buf_2);

    // Should have 5 chars for set of 4 + 1 for z + 2 terminators.
    EXPECT_THAT(dest_buf, ElementsAre(122, 33, 60, 78, 63, 43, 126, 62));
  }
  {
    // Try with 2 leftover zero bytes. Make sure we don't get a "z".
    const uint8_t src_buf_3[] = {1, 2, 3, 4, 0, 0};
    DataVector<uint8_t> dest_buf = BasicModule::A85Encode(src_buf_3);

    // Should have 5 chars for set of 4 + 3 for last 2 + 2 terminators.
    EXPECT_THAT(dest_buf, ElementsAre(33, 60, 78, 63, 43, 33, 33, 33, 126, 62));
  }
}

// Make sure we get returns in the expected locations.
TEST(fxcodec, A85LineBreaks) {
  // Make sure really big values don't break.
  std::array<uint8_t, 131> src_buf = {};
  // 1 full line + most of a line of normal symbols.
  for (int k = 0; k < 116; k += 4) {
    src_buf[k] = 1;
    src_buf[k + 1] = 2;
    src_buf[k + 2] = 3;
    src_buf[k + 3] = 4;
  }
  // Fill in the end, leaving an all zero gap + 3 extra zeros at the end.
  for (int k = 120; k < 128; k++) {
    src_buf[k] = 1;
    src_buf[k + 1] = 2;
    src_buf[k + 2] = 3;
    src_buf[k + 3] = 4;
  }

  // Should succeed.
  DataVector<uint8_t> dest_buf = BasicModule::A85Encode(src_buf);

  // Should have 75 chars in the first row plus 2 char return,
  // 76 chars in the second row plus 2 char return,
  // and 9 chars in the last row with 2 terminators.
  ASSERT_EQ(166u, dest_buf.size());

  // Check for the returns.
  EXPECT_EQ(13, dest_buf[75]);
  EXPECT_EQ(10, dest_buf[76]);
  EXPECT_EQ(13, dest_buf[153]);
  EXPECT_EQ(10, dest_buf[154]);
}
