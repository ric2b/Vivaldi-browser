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

#include <limits>

#include "absl/time/time.h"

namespace fcp {

google::protobuf::Timestamp TimeUtil::ConvertAbslToProtoTimestamp(
    absl::Time t) {
  google::protobuf::Timestamp proto_timestamp;
  const int64_t s = absl::ToUnixSeconds(t);
  proto_timestamp.set_seconds(s);
  // The nanos field can only range from 0 to 1e9 - 1 so conversion to int32 is
  // fine.
  proto_timestamp.set_nanos((t - absl::FromUnixSeconds(s)) /
                            absl::Nanoseconds(1));
  return proto_timestamp;
}

absl::Time TimeUtil::ConvertProtoToAbslTime(google::protobuf::Timestamp proto) {
  return absl::FromUnixSeconds(proto.seconds()) +
         absl::Nanoseconds(proto.nanos());
}

google::protobuf::Duration TimeUtil::ConvertAbslToProtoDuration(
    absl::Duration absl_duration) {
  google::protobuf::Duration proto_duration;
  if (absl_duration == absl::InfiniteDuration()) {
    proto_duration.set_seconds(std::numeric_limits<int64_t>::max());
    proto_duration.set_nanos(static_cast<int32_t>(999999999));
  } else if (absl_duration == -absl::InfiniteDuration()) {
    proto_duration.set_seconds(std::numeric_limits<int64_t>::min());
    proto_duration.set_nanos(static_cast<int32_t>(-999999999));
  } else {
    // s and n may both be negative, per the Duration proto spec.
    const int64_t s =
        absl::IDivDuration(absl_duration, absl::Seconds(1), &absl_duration);
    const int64_t n =
        absl::IDivDuration(absl_duration, absl::Nanoseconds(1), &absl_duration);
    proto_duration.set_seconds(s);
    proto_duration.set_nanos(static_cast<int32_t>(n));
  }
  return proto_duration;
}

absl::Duration TimeUtil::ConvertProtoToAbslDuration(
    google::protobuf::Duration proto) {
  return absl::Seconds(proto.seconds()) + absl::Nanoseconds(proto.nanos());
}

}  // namespace fcp
