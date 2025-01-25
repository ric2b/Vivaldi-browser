// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/public/statistics.h"

#include "gtest/gtest.h"
#include "util/std_util.h"

namespace openscreen::cast {

class StatisticsTest : public testing::Test {
 public:
  StatisticsTest() : test_histogram_(-80, 100, 20) {
    constexpr std::array<int64_t, 12> kDefaultSamples{
        {-200, -144, -80, -61, -60, 59, 0, 79, 80, 81, 99, 100}};
    for (int64_t sample : kDefaultSamples) {
      test_histogram_.Add(sample);
    }
  }

  const SimpleHistogram& test_histogram() { return test_histogram_; }

 protected:
  SimpleHistogram test_histogram_;
};

TEST_F(StatisticsTest, SimpleHistogramSamples) {
  EXPECT_EQ(-80, test_histogram().min);
  EXPECT_EQ(100, test_histogram().max);
  EXPECT_EQ(20, test_histogram().width);
  EXPECT_EQ(11u, test_histogram().buckets.size());

  constexpr std::array<std::pair<size_t, int64_t>, 7> kExpectedBuckets{
      {{0u, 2}, {1u, 2}, {2u, 1}, {7u, 1}, {8u, 1}, {9u, 3}, {10u, 1}}};
  for (const std::pair<size_t, int64_t>& bucket : kExpectedBuckets) {
    EXPECT_EQ(bucket.second, test_histogram().buckets[bucket.first]);
  }
}

TEST_F(StatisticsTest, SimpleHistogramCopy) {
  SimpleHistogram copy = test_histogram();
  EXPECT_EQ(test_histogram(), copy);
}

TEST_F(StatisticsTest, SimpleHistogramSerialization) {
  EXPECT_EQ("[]", SimpleHistogram().ToString());

  constexpr const char* kExpected =
      "[{\"<-80\":2},{\"-80--61\":2},{\"-60--41\":1},{\"0-19\":1},"
      "{\"40-59\":1},{\"60-79\":1},{\"80-99\":3},{\">=100\":1}]";

  std::string serialized = test_histogram().ToString();
  RemoveWhitespace(serialized);
  EXPECT_EQ(kExpected, serialized);
}

}  // namespace openscreen::cast
