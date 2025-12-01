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
#ifndef FCP_CLIENT_ENGINE_PRIVACY_ID_UTILS_H_
#define FCP_CLIENT_ENGINE_PRIVACY_ID_UTILS_H_

#include <cstdint>
#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/time/civil_time.h"
#include "fcp/client/example_query_result.pb.h"
#include "fcp/protos/confidentialcompute/windowing_schedule.pb.h"
#include "fcp/protos/plan.pb.h"

namespace fcp {
namespace client {
namespace engine {

// Deterministically derive a 16 byte privacy ID from the source ID and window
// start.
absl::StatusOr<std::string> GetPrivacyId(absl::string_view source_id,
                                         absl::CivilSecond window_start);

// ExampleQueryResult corresponding to a specific privacy ID.
struct PerPrivacyIdResult {
  std::string privacy_id;
  fcp::client::ExampleQueryResult example_query_result;
};

// An ExampleQuery and its results split by privacy ID.
struct SplitResults {
  google::internal::federated::plan::ExampleQuerySpec::ExampleQuery
      example_query;
  std::vector<PerPrivacyIdResult> per_privacy_id_results;
};

// Split the ExampleQueryResult into multiple ExampleQueryResults, one for each
// unique privacy ID.
// Requires that the ExampleQueryResult has an event time column with the
// kEventTimeColumnName. Event times should be in the format
// YYYY-MM-DDTHH:MM:SS[+-]HH:MM.
// ExampleQueryResult must not already have a column with the
// kPrivacyIdColumnName.
absl::StatusOr<SplitResults> SplitResultsByPrivacyId(
    google::internal::federated::plan::ExampleQuerySpec::ExampleQuery
        example_query,
    const fcp::client::ExampleQueryResult& example_query_result,
    const google::internal::federated::plan::PrivacyIdConfig& privacy_id_config,
    absl::string_view source_id);

// Returns the number of prefix bits to preserve in the partition key such that
// the partition key can represent at least num_partitions distinct values.
absl::StatusOr<int32_t> GetNumPrefixBits(int32_t num_partitions);

// Returns a partition key for the given privacy ID.
//
// The partition key is a hash of the privacy ID with num_prefix_bits preserved
// and the rest zeroed out.
absl::StatusOr<uint64_t> GetPartitionKey(int32_t num_prefix_bits,
                                         absl::string_view privacy_id);
}  // namespace engine
}  // namespace client
}  // namespace fcp

#endif  // FCP_CLIENT_ENGINE_PRIVACY_ID_UTILS_H_
