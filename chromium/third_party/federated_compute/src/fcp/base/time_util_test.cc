/*
 * Copyright 2022 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "fcp/base/time_util.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "fcp/testing/testing.h"

namespace fcp {
namespace {

TEST(ConvertAbslToProtoTimestampTest, ConvertSuccessfully) {
  absl::Time time = absl::FromUnixSeconds(1000) + absl::Nanoseconds(3);
  google::protobuf::Timestamp expected_timestamp;
  expected_timestamp.set_seconds(1000L);
  expected_timestamp.set_nanos(3);
  EXPECT_THAT(TimeUtil::ConvertAbslToProtoTimestamp(time),
              EqualsProto(expected_timestamp));
}

TEST(ConvertProtoToAbslTimeTest, ConvertSuccessfully) {
  google::protobuf::Timestamp timestamp;
  timestamp.set_seconds(1000L);
  timestamp.set_nanos(3);
  absl::Time expected_time = absl::FromUnixSeconds(1000) + absl::Nanoseconds(3);
  EXPECT_EQ(TimeUtil::ConvertProtoToAbslTime(timestamp), expected_time);
}

TEST(ConvertAbslToProtoDurationTest, ConvertSuccessfully) {
  absl::Duration duration = absl::Seconds(1000) + absl::Nanoseconds(3);
  google::protobuf::Duration expected_duration;
  expected_duration.set_seconds(1000L);
  expected_duration.set_nanos(3);
  EXPECT_THAT(TimeUtil::ConvertAbslToProtoDuration(duration),
              EqualsProto(expected_duration));
}

TEST(ConvertProtoToAbslDurationTest, ConvertSuccessfully) {
  google::protobuf::Duration duration;
  duration.set_seconds(1000L);
  duration.set_nanos(3);
  absl::Duration expected_duration = absl::Seconds(1000) + absl::Nanoseconds(3);
  EXPECT_EQ(TimeUtil::ConvertProtoToAbslDuration(duration), expected_duration);
}

}  // anonymous namespace
}  // namespace fcp
