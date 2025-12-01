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

#include "fcp/client/opstats/opstats_utils.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "google/protobuf/duration.pb.h"
#include "google/protobuf/timestamp.pb.h"
#include "google/protobuf/util/time_util.h"
#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "fcp/base/monitoring.h"
#include "fcp/protos/federated_api.pb.h"
#include "fcp/protos/opstats.pb.h"
#include "re2/re2.h"

namespace fcp {
namespace client {
namespace opstats {
namespace {

using ::google::internal::federatedml::v2::RetryWindow;
using ::google::protobuf::util::TimeUtil;

using PerTaskStats = std::vector<OperationalStats::PhaseStats>;

// Take an OperationalStats, split it into a list of PerTaskStats. Each
// PerTaskStats is a list of PhaseStats for a given task. For example, assuming
// we have gone through the following phases: Phase 1: ELIGIBILITY_EVAL_CHECKIN
// Phase 2: ELIGIBILITY_COMPUTATION
// Phase 3: MULTIPLE_TASK_ASSIGNMENTS (which returns 2 tasks, task 1 and task 2)
// Phase 4: COMPUTATION (task 1)
// Phase 5: UPLOAD (task 1)
// Phase 6: COMPUTATION (task 2)
// Phase 7: UPLOAD (task 2)
// Phase 8: CHECKIN (returns task 3)
// Phase 9: COMPUTATION (task 3)
// Phase 10: UPLOAD (task 3)
// Because the eligibility phases are shared between all 3 tasks, the PhaseStats
// will be duplicated for all 3 tasks. Similarly, task 1 and task 2 will share
// the same multiple task assignments PhaseStats. The returned PerTaskStats are:
// PerTaskStats 1 (task 1): Phase 1, 2, 3, 4, 5
// PerTaskStats 2 (task 2): Phase 1, 2, 3, 6, 7
// PerTaskStats 3 (task 3): Phase 1, 2, 8, 9, 10
std::vector<PerTaskStats> GeneratePerTaskStats(
    const OperationalStats& op_stats) {
  std::vector<PerTaskStats> stats;
  std::vector<OperationalStats::PhaseStats> pre_task_assignment_stats;
  OperationalStats::PhaseStats latest_task_assignment_stats;
  PerTaskStats pending_stats;
  for (const auto& phase_stat : op_stats.phase_stats()) {
    switch (phase_stat.phase()) {
      case OperationalStats::PhaseStats::ELIGIBILITY_EVAL_CHECKIN:
      case OperationalStats::PhaseStats::ELIGIBILITY_COMPUTATION:
        pre_task_assignment_stats.push_back(phase_stat);
        break;
      case OperationalStats::PhaseStats::MULTIPLE_TASK_ASSIGNMENTS:
      case OperationalStats::PhaseStats::CHECKIN:
        latest_task_assignment_stats = phase_stat;
        break;
      case OperationalStats::PhaseStats::COMPUTATION:
        if (!pending_stats.empty()) {
          stats.push_back(pending_stats);
        }
        pending_stats = PerTaskStats(pre_task_assignment_stats.begin(),
                                     pre_task_assignment_stats.end());
        pending_stats.push_back(latest_task_assignment_stats);
        pending_stats.push_back(phase_stat);
        break;
      case OperationalStats::PhaseStats::UPLOAD:
        pending_stats.push_back(phase_stat);
        break;
      default:
        break;
    }
  }
  if (!pending_stats.empty()) {
    // Handle the PhaseStats for the last task.
    stats.push_back(pending_stats);
  }
  if (stats.empty()) {
    // If we reach this line, it means we never get into the computation phase
    // in this run. We'll collect the pre_task_assignment_stats and
    // latest_task_assignment_stats if they are not empty. We don't need to do
    // this if we reach computation phase at least once.
    if (!pre_task_assignment_stats.empty()) {
      pending_stats = std::move(pre_task_assignment_stats);
    }
    if (!latest_task_assignment_stats.events().empty()) {
      pending_stats.push_back(std::move(latest_task_assignment_stats));
    }
    stats.push_back(pending_stats);
  }
  return stats;
}

// Creates a legacy OperationalStats (without PhaseStats) based on PerTaskStats
// and other inputs.
OperationalStats CreateLegacyOperationalStats(
    absl::string_view population_name, absl::string_view session_name,
    const RetryWindow& retry_window, const PerTaskStats& per_task_stats) {
  OperationalStats legacy_stats;
  legacy_stats.set_population_name(std::string(population_name));
  legacy_stats.set_session_name(std::string(session_name));
  if (retry_window.has_delay_max() || retry_window.has_delay_min()) {
    *legacy_stats.mutable_retry_window() = retry_window;
  }
  int64_t bytes_downloaded = 0;
  int64_t bytes_uploaded = 0;
  int64_t network_duration_millis = 0;
  for (const auto& phase_stats : per_task_stats) {
    if (!phase_stats.task_name().empty()) {
      legacy_stats.set_task_name(phase_stats.task_name());
    }
    if (!phase_stats.error_message().empty()) {
      legacy_stats.set_error_message(phase_stats.error_message());
    }
    if (phase_stats.has_min_sep_policy_index() &&
        !legacy_stats.has_min_sep_policy_index()) {
      legacy_stats.set_min_sep_policy_index(phase_stats.min_sep_policy_index());
    }
    for (const auto& [collection_uri, stats] : phase_stats.dataset_stats()) {
      auto& existing_dataset_stats =
          (*legacy_stats.mutable_dataset_stats())[collection_uri];
      existing_dataset_stats.set_num_bytes_read(
          existing_dataset_stats.num_bytes_read() + stats.num_bytes_read());
      existing_dataset_stats.set_num_examples_read(
          existing_dataset_stats.num_examples_read() +
          stats.num_examples_read());
      if (!existing_dataset_stats.has_first_access_timestamp() ||
          stats.first_access_timestamp() <
              existing_dataset_stats.first_access_timestamp()) {
        *existing_dataset_stats.mutable_first_access_timestamp() =
            stats.first_access_timestamp();
      }
    }
    for (const auto& event : phase_stats.events()) {
      *legacy_stats.add_events() = event;
    }
    // Note these metrics are per phase numbers in PhaseStats, when we set the
    // top level corresponding fields, we need to use the aggregated numbers.
    bytes_downloaded += phase_stats.bytes_downloaded();
    bytes_uploaded += phase_stats.bytes_uploaded();
    network_duration_millis +=
        TimeUtil::DurationToMilliseconds(phase_stats.network_duration());
  }
  legacy_stats.set_chunking_layer_bytes_downloaded(bytes_downloaded);
  legacy_stats.set_chunking_layer_bytes_uploaded(bytes_uploaded);
  if (network_duration_millis != 0) {
    *legacy_stats.mutable_network_duration() =
        TimeUtil::MillisecondsToDuration(network_duration_millis);
  }
  return legacy_stats;
}

// Convert an OperationalStats to a list of legacy OperationalStats (without
// PhaseStats).  If the input OperationalStats is already a legacy
// OperationalStats, it will return a list of single element which is the input
// OperationalStats. When the input OperationalStats has PhaseStats, it
// potentially will be split into several legacy OperationalStats. Each legacy
// OperationalStats contains all the metrics for a single task.
std::vector<OperationalStats> ConvertToLegacyOperationalStats(
    const OperationalStats& op_stats) {
  std::vector<OperationalStats> legacy_op_stats;
  if (op_stats.phase_stats().empty()) {
    legacy_op_stats.push_back(op_stats);
  } else {
    std::vector<PerTaskStats> task_stats_list = GeneratePerTaskStats(op_stats);
    for (const auto& task_stats : task_stats_list) {
      legacy_op_stats.push_back(CreateLegacyOperationalStats(
          op_stats.population_name(), op_stats.session_name(),
          op_stats.retry_window(), task_stats));
    }
  }
  return legacy_op_stats;
}

// Convert all of the OperationalStats inside a OpStatsSequence into legacy
// OperationalStats (represent single task, no PhaseStats).
std::vector<OperationalStats> ConvertToLegacyOperationalStats(
    const OpStatsSequence& data) {
  std::vector<OperationalStats> legacy_op_stats;
  for (const auto& op_stats : data.opstats()) {
    std::vector<OperationalStats> converted_op_stats =
        ConvertToLegacyOperationalStats(op_stats);
    for (const auto& stats : converted_op_stats) {
      legacy_op_stats.push_back(stats);
    }
  }
  return legacy_op_stats;
}

// This method only works on legacy OperationalStats without PhaseStats.
std::optional<OperationalStats> GetLastSuccessfulContributionForPredicate(
    const std::vector<OperationalStats>& data,
    std::function<bool(const OperationalStats&)> predicate) {
  for (auto it = data.rbegin(); it != data.rend(); ++it) {
    const OperationalStats& opstats_entry = *it;
    FCP_CHECK(opstats_entry.phase_stats().empty())
        << "OperationalStats with PhaseStats is not supported in this method. "
           "Please convert it to legacy OperationalStats before calling this "
           "method.";
    bool upload_started = false;
    bool upload_aborted = false;
    if (!predicate(opstats_entry)) {
      continue;
    }
    for (const auto& event : opstats_entry.events()) {
      if (event.event_type() ==
          OperationalStats::Event::EVENT_KIND_RESULT_UPLOAD_STARTED) {
        upload_started = true;
      }
      if (event.event_type() ==
          OperationalStats::Event::EVENT_KIND_RESULT_UPLOAD_SERVER_ABORTED) {
        upload_aborted = true;
      }
    }
    if (upload_started && !upload_aborted) {
      return opstats_entry;
    }
  }
  return std::nullopt;
}

// We use the timestamp of the RESULT_UPLOAD_STARTED event for the contribution
// time.
std::optional<google::protobuf::Timestamp> GetContributionTimeForLegacyOpStats(
    const OperationalStats& entry) {
  auto upload_started = std::find_if(
      entry.events().begin(), entry.events().end(),
      [](const OperationalStats::Event& event) {
        return event.event_type() ==
               OperationalStats::Event::EVENT_KIND_RESULT_UPLOAD_STARTED;
      });
  if (upload_started == entry.events().end()) {
    // For last_successful_entry to have a value, it must have had an
    // EVENT_KIND_RESULT_UPLOAD_STARTED event, so we should never reach this.
    return std::nullopt;
  }

  return upload_started->timestamp();
}

absl::flat_hash_map<std::string, google::protobuf::Timestamp>
GetCollectionFirstAccessTimeFromLegacyOpstats(const OperationalStats& entry) {
  absl::flat_hash_map<std::string, google::protobuf::Timestamp>
      collection_first_access_times;
  for (const auto& [collection_uri, stats] : entry.dataset_stats()) {
    collection_first_access_times[collection_uri] =
        stats.first_access_timestamp();
  }
  return collection_first_access_times;
}

absl::Time GetLastUpdatedTimeFromLegacyOpStats(
    const OperationalStats& op_stats) {
  if (op_stats.events().empty()) {
    return absl::InfinitePast();
  } else {
    return absl::FromUnixMillis(TimeUtil::TimestampToMilliseconds(
        op_stats.events().rbegin()->timestamp()));
  }
}

}  // anonymous namespace

// Note that this method returns a legacy OperationalStats converted from the
// input OperationalStats if the last successful contribution is found (see
// ConvertToLegacyOperationalStats for more details). Make sure the information
// in the returned OperationalStats is sufficient for your use case.
std::optional<OperationalStats> GetLastSuccessfulContribution(
    const OpStatsSequence& data, absl::string_view task_name) {
  std::vector<OperationalStats> legacy_op_stats =
      ConvertToLegacyOperationalStats(data);
  return GetLastSuccessfulContributionForPredicate(
      legacy_op_stats, [task_name](const OperationalStats& opstats_entry) {
        return opstats_entry.task_name() == task_name;
      });
}

std::optional<absl::flat_hash_map<std::string, google::protobuf::Timestamp>>
GetPreviousCollectionFirstAccessTimeMap(const OpStatsSequence& data,
                                        absl::string_view task_name) {
  std::optional<OperationalStats> last_successful_entry =
      GetLastSuccessfulContribution(data, task_name);

  if (!last_successful_entry.has_value()) {
    return std::nullopt;
  }

  return GetCollectionFirstAccessTimeFromLegacyOpstats(*last_successful_entry);
}

std::optional<google::protobuf::Timestamp> GetLastSuccessfulContributionTime(
    const OpStatsSequence& data, absl::string_view task_name) {
  std::optional<OperationalStats> last_successful_entry =
      GetLastSuccessfulContribution(data, task_name);

  if (!last_successful_entry.has_value()) {
    return std::nullopt;
  }

  return GetContributionTimeForLegacyOpStats(*last_successful_entry);
}

std::optional<google::protobuf::Timestamp>
GetLastSuccessfulContributionTimeForPattern(const OpStatsSequence& data,
                                            const RE2& compiled_pattern) {
  std::vector<OperationalStats> legacy_op_stats =
      ConvertToLegacyOperationalStats(data);
  std::optional<OperationalStats> last_successful_entry =
      GetLastSuccessfulContributionForPredicate(
          legacy_op_stats,
          [&compiled_pattern](const OperationalStats& opstats_entry) {
            return RE2::FullMatch(opstats_entry.task_name(), compiled_pattern);
          });

  if (!last_successful_entry.has_value()) {
    return std::nullopt;
  }

  return GetContributionTimeForLegacyOpStats(*last_successful_entry);
}

std::vector<OperationalStats> GetOperationalStatsForTimeRange(
    const OpStatsSequence& data, absl::Time lower_bound_time,
    absl::Time upper_bound_time) {
  std::vector<OperationalStats> selected_data;
  std::vector<OperationalStats> legacy_op_stats =
      ConvertToLegacyOperationalStats(data);
  for (auto it = legacy_op_stats.rbegin(); it != legacy_op_stats.rend(); ++it) {
    absl::Time last_update_time = GetLastUpdatedTimeFromLegacyOpStats(*it);
    if (last_update_time >= lower_bound_time &&
        last_update_time <= upper_bound_time) {
      selected_data.push_back(*it);
    }
  }
  return selected_data;
}

std::optional<int64_t> GetLastSuccessfulContributionMinSepPolicyIndex(
    const OpStatsSequence& data, absl::string_view task_name) {
  std::optional<OperationalStats> last_successful_entry =
      GetLastSuccessfulContribution(data, task_name);

  // Note that it is possible that there was a successful contribution, but the
  // opstats db got corrupted/deleted within the minimum separation period, so
  // the client no longer has the entry indicating their last successful
  // contribution.
  if (!last_successful_entry.has_value() ||
      !last_successful_entry->has_min_sep_policy_index()) {
    return std::nullopt;
  }

  return last_successful_entry->min_sep_policy_index();
}

}  // namespace opstats
}  // namespace client
}  // namespace fcp
