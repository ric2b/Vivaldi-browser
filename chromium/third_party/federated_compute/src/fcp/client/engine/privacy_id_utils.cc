/*
 * Copyright 2025 Google LLC
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
#include "fcp/client/engine/privacy_id_utils.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "google/type/datetime.pb.h"
#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/time/civil_time.h"
#include "fcp/base/digest.h"
#include "fcp/base/monitoring.h"
#include "fcp/client/event_time_range.pb.h"
#include "fcp/confidentialcompute/constants.h"
#include "fcp/confidentialcompute/time_window_utilities.h"
#include "fcp/protos/confidentialcompute/windowing_schedule.pb.h"
#include "fcp/protos/plan.pb.h"

namespace fcp {
namespace client {
namespace engine {

namespace {

using ::fcp::confidentialcompute::WindowingSchedule;
using ::google::internal::federated::plan::ExampleQuerySpec;
using ::google::internal::federated::plan::PrivacyIdConfig;

constexpr int kPrivacyIdLength = 16;

// Remove the timezone modifier from the event time. Event_time must be in the
// format YYYY-MM-DDTHH:MM:SS[+-]HH:MM, and the result will be in the format
// YYYY-MM-DDTHH:MM:SS.
absl::StatusOr<std::string> RemoveTimezoneFromEventTime(
    absl::string_view event_time) {
  if (event_time.length() != 25) {
    return absl::InvalidArgumentError(
        "Invalid event time format: incorrect length");
  }
  // Basic check for the presence of 'T' and timezone sign.
  if (event_time[10] != 'T' ||
      (event_time[19] != '+' && event_time[19] != '-')) {
    return absl::InvalidArgumentError(
        "Invalid event time format: missing T or timezone modifier");
  }
  return std::string(event_time.substr(0, 19));
}

absl::StatusOr<WindowingSchedule::CivilTimeWindowSchedule>
VerifyConfigHasCivilTimeWindowSchedule(
    const PrivacyIdConfig& privacy_id_config) {
  if (!privacy_id_config.has_windowing_schedule() ||
      !privacy_id_config.windowing_schedule()
           .has_civil_time_window_schedule()) {
    return absl::InvalidArgumentError("Missing CivilTimeWindowSchedule");
  }
  return privacy_id_config.windowing_schedule().civil_time_window_schedule();
}

template <typename ValuesType>
void CopySelectedValues(const ValuesType& source, ValuesType* dest,
                        const std::vector<int>& indices) {
  for (int index : indices) {
    dest->add_value(source.value(index));
  }
}

google::type::DateTime ConvertCivilHourToDateTime(absl::CivilHour civil_hour) {
  google::type::DateTime date_time;
  date_time.set_year(static_cast<int32_t>(civil_hour.year()));
  date_time.set_month(civil_hour.month());
  date_time.set_day(civil_hour.day());
  date_time.set_hours(civil_hour.hour());
  return date_time;
}

struct ExampleQueryResultSelection {
  std::vector<int> indices;
  // Use CivilHour as the finest granularity since this range will be stored in
  // the unencrypted blob header.
  absl::CivilHour min_event_time = absl::CivilHour::max();
  absl::CivilHour max_event_time = absl::CivilHour::min();
};

absl::StatusOr<ExampleQueryResult> CreateExampleQueryResultFromSelection(
    const ExampleQueryResult& original_result,
    const ExampleQueryResultSelection& selection) {
  ExampleQueryResult new_result;
  for (const auto& [column_name, values] :
       original_result.vector_data().vectors()) {
    ExampleQueryResult::VectorData::Values new_values;
    switch (values.values_case()) {
      case ExampleQueryResult::VectorData::Values::kInt32Values:
        CopySelectedValues(values.int32_values(),
                           new_values.mutable_int32_values(),
                           selection.indices);
        break;
      case ExampleQueryResult::VectorData::Values::kInt64Values:
        CopySelectedValues(values.int64_values(),
                           new_values.mutable_int64_values(),
                           selection.indices);
        break;
      case ExampleQueryResult::VectorData::Values::kStringValues:
        CopySelectedValues(values.string_values(),
                           new_values.mutable_string_values(),
                           selection.indices);
        break;
      case ExampleQueryResult::VectorData::Values::kBoolValues:
        CopySelectedValues(values.bool_values(),
                           new_values.mutable_bool_values(), selection.indices);
        break;
      case ExampleQueryResult::VectorData::Values::kFloatValues:
        CopySelectedValues(values.float_values(),
                           new_values.mutable_float_values(),
                           selection.indices);
        break;
      case ExampleQueryResult::VectorData::Values::kDoubleValues:
        CopySelectedValues(values.double_values(),
                           new_values.mutable_double_values(),
                           selection.indices);
        break;
      case ExampleQueryResult::VectorData::Values::kBytesValues:
        CopySelectedValues(values.bytes_values(),
                           new_values.mutable_bytes_values(),
                           selection.indices);
        break;
      case ExampleQueryResult::VectorData::Values::VALUES_NOT_SET:
        return absl::InvalidArgumentError(
            "Invalid values type in the example query result");
    }
    (*new_result.mutable_vector_data()->mutable_vectors())[column_name] =
        new_values;
  }
  new_result.mutable_stats()->set_output_rows_count(
      static_cast<int32_t>(selection.indices.size()));
  if (!selection.indices.empty()) {
    *new_result.mutable_stats()
         ->mutable_cross_query_event_time_range()
         ->mutable_start_event_time() =
        ConvertCivilHourToDateTime(selection.min_event_time);
    *new_result.mutable_stats()
         ->mutable_cross_query_event_time_range()
         ->mutable_end_event_time() =
        ConvertCivilHourToDateTime(selection.max_event_time);
  }
  return new_result;
}

absl::StatusOr<absl::CivilSecond> ConvertEventTimeToCivilSecond(
    absl::string_view event_time) {
  FCP_ASSIGN_OR_RETURN(std::string event_time_without_timezone,
                       RemoveTimezoneFromEventTime(event_time));
  absl::CivilSecond event_civil_second;
  if (!absl::ParseCivilTime(event_time_without_timezone, &event_civil_second)) {
    return absl::InvalidArgumentError("Invalid event time format");
  }
  return event_civil_second;
}

// Loads a uint64_t from the given buffer in big-endian order.
// TODO: b/439902489 - Replace this with the CFC version once it's available.
uint64_t Load64BigEndian(const char* buf) {
  return (static_cast<uint64_t>(static_cast<unsigned char>(buf[0])) << 56) |
         (static_cast<uint64_t>(static_cast<unsigned char>(buf[1])) << 48) |
         (static_cast<uint64_t>(static_cast<unsigned char>(buf[2])) << 40) |
         (static_cast<uint64_t>(static_cast<unsigned char>(buf[3])) << 32) |
         (static_cast<uint64_t>(static_cast<unsigned char>(buf[4])) << 24) |
         (static_cast<uint64_t>(static_cast<unsigned char>(buf[5])) << 16) |
         (static_cast<uint64_t>(static_cast<unsigned char>(buf[6])) << 8) |
         static_cast<uint64_t>(static_cast<unsigned char>(buf[7]));
}
}  // namespace

absl::StatusOr<std::string> GetPrivacyId(absl::string_view source_id,
                                         absl::CivilSecond window_start) {
  std::string hash = ComputeSHA256(
      absl::StrCat(source_id, absl::FormatCivilTime(window_start)));
  if (hash.size() < kPrivacyIdLength) {
    return absl::InternalError("SHA256 hash is too short");
  }
  return hash.substr(0, kPrivacyIdLength);
}

absl::StatusOr<SplitResults> SplitResultsByPrivacyId(
    ExampleQuerySpec::ExampleQuery example_query,
    const ExampleQueryResult& example_query_result,
    const PrivacyIdConfig& privacy_id_config, absl::string_view source_id) {
  const ExampleQueryResult::VectorData::Values* event_time_values = nullptr;

  for (const auto& [column_name, values] :
       example_query_result.vector_data().vectors()) {
    if (absl::EndsWith(column_name,
                       confidential_compute::kEventTimeColumnName)) {
      if (event_time_values != nullptr) {
        return absl::InvalidArgumentError(
            "Multiple columns found ending with " +
            std::string(confidential_compute::kEventTimeColumnName));
      }
      event_time_values = &values;
    }
  }

  if (event_time_values == nullptr) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Required column ending with %s not found in the example query result",
        confidential_compute::kEventTimeColumnName));
  }

  if (!event_time_values->has_string_values()) {
    return absl::InvalidArgumentError(
        "Unexpected data type for event time column, expected string.");
  }
  if (example_query_result.vector_data().vectors().contains(
          confidential_compute::kPrivacyIdColumnName)) {
    return absl::InvalidArgumentError(
        "Privacy ID column cannot already exist in the example query result");
  }

  // Build a map of privacy ID to the indices of the rows with that privacy
  // ID. Also track the earliest and latest event times for each privacy ID,
  // since we need to set the event time ranges for the per privacy ID results.
  absl::flat_hash_map<std::string, ExampleQueryResultSelection>
      privacy_id_selections;
  for (int i = 0; i < event_time_values->string_values().value_size(); ++i) {
    const std::string& event_time_str =
        event_time_values->string_values().value(i);
    FCP_ASSIGN_OR_RETURN(absl::CivilSecond event_civil_second,
                         ConvertEventTimeToCivilSecond(event_time_str));

    FCP_ASSIGN_OR_RETURN(
        WindowingSchedule::CivilTimeWindowSchedule schedule,
        VerifyConfigHasCivilTimeWindowSchedule(privacy_id_config));
    FCP_ASSIGN_OR_RETURN(absl::CivilSecond window_start,
                         GetTimeWindowStart(schedule, event_civil_second));
    FCP_ASSIGN_OR_RETURN(std::string privacy_id,
                         GetPrivacyId(source_id, window_start));

    ExampleQueryResultSelection& selection = privacy_id_selections[privacy_id];
    selection.indices.push_back(i);
    selection.min_event_time =
        std::min(absl::CivilHour(event_civil_second), selection.min_event_time);
    selection.max_event_time =
        std::max(absl::CivilHour(event_civil_second), selection.max_event_time);
  }

  SplitResults split_results = {.example_query = std::move(example_query)};
  for (const auto& [privacy_id, selection] : privacy_id_selections) {
    PerPrivacyIdResult per_privacy_id_result;
    per_privacy_id_result.privacy_id = privacy_id;
    FCP_ASSIGN_OR_RETURN(
        ExampleQueryResult per_privacy_id_example_query_result,
        CreateExampleQueryResultFromSelection(example_query_result, selection));

    per_privacy_id_result.example_query_result =
        std::move(per_privacy_id_example_query_result);

    split_results.per_privacy_id_results.push_back(
        std::move(per_privacy_id_result));
  }
  return split_results;
}

absl::StatusOr<int32_t> GetNumPrefixBits(int32_t num_partitions) {
  if (num_partitions <= 0) {
    return absl::InvalidArgumentError("num_partitions must be positive.");
  }
  return static_cast<int32_t>(std::ceil(std::log2(num_partitions)));
}

absl::StatusOr<uint64_t> GetPartitionKey(int32_t num_prefix_bits,
                                         absl::string_view privacy_id) {
  if (num_prefix_bits > 64) {
    return absl::InvalidArgumentError("num_prefix_bits must be <= 64.");
  }
  if (num_prefix_bits < 0) {
    return absl::InvalidArgumentError("num_prefix_bits must be non-negative.");
  }
  if (num_prefix_bits == 0) {
    // If no bits are preserved, the key is all zeros.
    return 0;
  }

  int32_t num_bits_to_zero = 64 - num_prefix_bits;
  // Hash the privacy ID to ensure partition keys are uniformly distributed.
  std::string partition_key = ComputeSHA256(privacy_id);
  partition_key = partition_key.substr(0, 8);

  uint64_t partition_key_uint64 = Load64BigEndian(partition_key.data());
  // Create a mask with the top num_prefix_bits set to 1.
  uint64_t mask = ~0ULL << num_bits_to_zero;
  return partition_key_uint64 & mask;
}

}  // namespace engine
}  // namespace client
}  // namespace fcp
