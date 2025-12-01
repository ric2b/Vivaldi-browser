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

#ifndef FCP_CLIENT_OPSTATS_OPSTATS_UTILS_H_
#define FCP_CLIENT_OPSTATS_OPSTATS_UTILS_H_

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "google/protobuf/timestamp.pb.h"
#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "fcp/protos/opstats.pb.h"
#include "re2/re2.h"

namespace fcp {
namespace client {
namespace opstats {

// Returns an optional containing a map of collection URIs to the timestamp of
// the first time the collection was accessed when the runtime last
// successfully contributed to a task.
std::optional<absl::flat_hash_map<std::string, google::protobuf::Timestamp>>
GetPreviousCollectionFirstAccessTimeMap(const OpStatsSequence& data,
                                        absl::string_view task_name);

// Returns an optional containing an OperationalStats of the last time the
// runtime successfully contributed to a task with the given task name,
// otherwise returns an empty optional.
std::optional<OperationalStats> GetLastSuccessfulContribution(
    const OpStatsSequence& data, absl::string_view task_name);

// Returns an optional containing the timestamp of the last time the runtime
// successfully contributed to a task with the given task name, otherwise
// returns an empty optional.
std::optional<google::protobuf::Timestamp> GetLastSuccessfulContributionTime(
    const OpStatsSequence& data, absl::string_view task_name);

// Returns an optional containing the timestamp of the last time the runtime
// successfully contributed to a task with a task name matching the given
// compiled_pattern, otherwise returns an empty optional. The compiled_pattern
// must be error-free.
std::optional<google::protobuf::Timestamp>
GetLastSuccessfulContributionTimeForPattern(const OpStatsSequence& data,
                                            const RE2& compiled_pattern);

// Returns an optional containing the MinimumSeparationPolicy index of the last
// time the runtime successfully contributed to a task with the given task name,
// otherwise returns an empty optional.
std::optional<int64_t> GetLastSuccessfulContributionMinSepPolicyIndex(
    const OpStatsSequence& data, absl::string_view task_name);

// Returns a list of OperationalStats for the tasks that ran in the given time
// range in reverse time order.
std::vector<OperationalStats> GetOperationalStatsForTimeRange(
    const OpStatsSequence& data, absl::Time lower_bound_time,
    absl::Time upper_bound_time);

}  // namespace opstats
}  // namespace client
}  // namespace fcp

#endif  // FCP_CLIENT_OPSTATS_OPSTATS_UTILS_H_
