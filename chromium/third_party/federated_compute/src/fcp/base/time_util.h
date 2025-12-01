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
#ifndef FCP_BASE_TIME_UTIL_H_
#define FCP_BASE_TIME_UTIL_H_

#include "google/protobuf/duration.pb.h"
#include "google/protobuf/timestamp.pb.h"
#include "absl/time/time.h"

namespace fcp {

class TimeUtil {
 public:
  // Converts an absl::Time to a google::protobuf::Timestamp.
  // Note that we assume the timestamps we deal with here are representable by
  // both formats. If the resulted google::protobuf::Timestamp is invalid, it
  // will lead to undefined behavior.
  static google::protobuf::Timestamp ConvertAbslToProtoTimestamp(absl::Time t);

  // Converts a google::protobuf::Timestamp to an absl::Time.
  // Note that we assume the timestamps we deal with here are representable by
  // both formats. If the resulted absl::Time is invalid, it will lead to
  // undefined behavior.
  static absl::Time ConvertProtoToAbslTime(google::protobuf::Timestamp proto);

  // Converts an absl::Duration to a google::protobuf::Duration.
  // Note that we assume the durations we deal with here are representable by
  // both formats. If the resulted google::protobuf::Duration is invalid, it
  // will lead to undefined behavior.
  static google::protobuf::Duration ConvertAbslToProtoDuration(
      absl::Duration absl_duration);

  // Converts a google::protobuf::Duration to an absl::Duration.
  // Note that we assume the timestamps we deal with here are representable by
  // both formats. If the resulted google::protobuf::Duration is invalid, it
  // will lead to undefined behavior.
  static absl::Duration ConvertProtoToAbslDuration(
      google::protobuf::Duration proto);
};

}  // namespace fcp

#endif  // FCP_BASE_TIME_UTIL_H_
