/*
 * Copyright 2021 Google LLC
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
#include "fcp/client/opstats/opstats_example_store.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "google/protobuf/any.pb.h"
#include "google/protobuf/util/time_util.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/time/time.h"
#include "fcp/base/monitoring.h"
#include "fcp/client/diag_codes.pb.h"
#include "fcp/client/opstats/opstats_utils.h"
#include "fcp/client/simple_task_environment.h"
#include "fcp/protos/federated_api.pb.h"
#include "fcp/protos/opstats.pb.h"
#include "tensorflow/core/example/example.pb.h"
#include "tensorflow/core/example/feature.pb.h"

namespace fcp {
namespace client {
namespace opstats {

using ::google::internal::federated::plan::ExampleSelector;
using ::google::protobuf::util::TimeUtil;

namespace {

tensorflow::Feature CreateFeatureFromString(const std::string& str) {
  tensorflow::Feature feature;
  feature.mutable_bytes_list()->add_value(str);
  return feature;
}

tensorflow::Feature CreateFeatureFromInt(int64_t value) {
  tensorflow::Feature feature;
  feature.mutable_int64_list()->add_value(value);
  return feature;
}

tensorflow::Feature CreateFeatureFromStringVector(
    const std::vector<std::string>& values) {
  tensorflow::Feature feature;
  auto* bytes_list = feature.mutable_bytes_list();
  for (const auto& value : values) {
    bytes_list->add_value(value);
  }
  return feature;
}

tensorflow::Feature CreateFeatureFromIntVector(
    const std::vector<int64_t>& values) {
  tensorflow::Feature feature;
  auto* int64_list = feature.mutable_int64_list();
  for (const auto& value : values) {
    int64_list->add_value(value);
  }
  return feature;
}

std::string CreateExample(const OperationalStats& op_stats,
                          int64_t earliest_trustworthy_time) {
  tensorflow::Example example;
  auto* feature_map = example.mutable_features()->mutable_feature();
  (*feature_map)[kPopulationName] =
      CreateFeatureFromString(op_stats.population_name());
  (*feature_map)[kSessionName] =
      CreateFeatureFromString(op_stats.session_name());
  (*feature_map)[kTaskName] = CreateFeatureFromString(op_stats.task_name());

  // Create events related features.
  std::vector<int64_t> event_types;
  std::vector<int64_t> event_time_millis;
  for (const auto& event : op_stats.events()) {
    event_types.push_back(event.event_type());
    event_time_millis.push_back(
        TimeUtil::TimestampToMilliseconds(event.timestamp()));
  }
  (*feature_map)[kEventsEventType] = CreateFeatureFromIntVector(event_types);
  (*feature_map)[kEventsTimestampMillis] =
      CreateFeatureFromIntVector(event_time_millis);

  // Create external dataset stats related features.
  std::vector<std::string> uris;
  std::vector<int64_t> num_examples_read;
  std::vector<int64_t> num_bytes_read;
  std::vector<int64_t> first_access_time_millis;
  for (const auto& stats : op_stats.dataset_stats()) {
    uris.push_back(stats.first);
    num_examples_read.push_back(stats.second.num_examples_read());
    num_bytes_read.push_back(stats.second.num_bytes_read());
    first_access_time_millis.push_back(TimeUtil::TimestampToMilliseconds(
        stats.second.first_access_timestamp()));
  }
  (*feature_map)[kDatasetStatsUri] = CreateFeatureFromStringVector(uris);
  (*feature_map)[kDatasetStatsNumExamplesRead] =
      CreateFeatureFromIntVector(num_examples_read);
  (*feature_map)[kDatasetStatsNumBytesRead] =
      CreateFeatureFromIntVector(num_bytes_read);

  (*feature_map)[kErrorMessage] =
      CreateFeatureFromString(op_stats.error_message());

  // Create RetryWindow related features.
  (*feature_map)[kRetryWindowDelayMinMillis] = CreateFeatureFromInt(
      TimeUtil::DurationToMilliseconds(op_stats.retry_window().delay_min()));
  (*feature_map)[kRetryWindowDelayMaxMillis] = CreateFeatureFromInt(
      TimeUtil::DurationToMilliseconds(op_stats.retry_window().delay_max()));

  (*feature_map)[kChunkingLayerBytesDownloaded] =
      CreateFeatureFromInt(op_stats.chunking_layer_bytes_downloaded());
  (*feature_map)[kChunkingLayerBytesUploaded] =
      CreateFeatureFromInt(op_stats.chunking_layer_bytes_uploaded());
    (*feature_map)[kNetworkDuration] = CreateFeatureFromInt(
        TimeUtil::DurationToMilliseconds(op_stats.network_duration()));

  (*feature_map)[kEarliestTrustWorthyTimeMillis] =
      CreateFeatureFromInt(earliest_trustworthy_time);

  (*feature_map)[kSupportsNeetContext] = CreateFeatureFromInt(1);

  return example.SerializeAsString();
}

class OpStatsExampleIterator : public fcp::client::ExampleIterator {
 public:
  explicit OpStatsExampleIterator(std::vector<OperationalStats> op_stats,
                                  int64_t earliest_trustworthy_time)
      : next_(0),
        data_(std::move(op_stats)),
        earliest_trustworthy_time_millis_(earliest_trustworthy_time) {}
  absl::StatusOr<std::string> Next() override {
    if (data_.empty() && next_ == 0) {
      // If there's no data, we still need to return an opstats example
      // containing baseline info like the earliest trustworthy time, and
      // NEET custom policy support. We'll increment next so we never hit this
      // case again.
      next_++;
      return CreateExample(OperationalStats(),
                           earliest_trustworthy_time_millis_);
    }

    if (next_ < 0 || next_ >= data_.size()) {
      return absl::OutOfRangeError("The iterator is out of range.");
    }
    return CreateExample(data_[next_++], earliest_trustworthy_time_millis_);
  }

  void Close() override {
    next_ = 0;
    data_.clear();
  }

 private:
  // The index for the next OperationalStats to be used.
  int next_;
  std::vector<OperationalStats> data_;
  const int64_t earliest_trustworthy_time_millis_;
};

}  // anonymous namespace

bool OpStatsExampleIteratorFactory::CanHandle(
    const ExampleSelector& example_selector) {
  return example_selector.collection_uri() == opstats::kOpStatsCollectionUri;
}

absl::StatusOr<std::unique_ptr<fcp::client::ExampleIterator>>
OpStatsExampleIteratorFactory::CreateExampleIterator(
    const ExampleSelector& example_selector) {
  if (example_selector.collection_uri() != kOpStatsCollectionUri) {
    log_manager_->LogDiag(ProdDiagCode::OPSTATS_INCORRECT_COLLECTION_URI);
    return absl::InvalidArgumentError(absl::StrCat(
        "The collection uri is ", example_selector.collection_uri(),
        ", which is not the expected uri: ", kOpStatsCollectionUri));
  }

  absl::Time lower_bound_time = absl::InfinitePast();
  absl::Time upper_bound_time = absl::InfiniteFuture();
  bool last_successful_contribution = false;
  if (example_selector.has_criteria()) {
    OpStatsSelectionCriteria criteria;
    if (!example_selector.criteria().UnpackTo(&criteria)) {
      log_manager_->LogDiag(ProdDiagCode::OPSTATS_INVALID_SELECTION_CRITERIA);
      return absl::InvalidArgumentError("Unable to parse selection criteria.");
    }

    if (criteria.has_start_time()) {
      lower_bound_time = absl::FromUnixMillis(
          TimeUtil::TimestampToMilliseconds(criteria.start_time()));
    }
    if (criteria.has_end_time()) {
      upper_bound_time = absl::FromUnixMillis(
          TimeUtil::TimestampToMilliseconds(criteria.end_time()));
    }
    if (lower_bound_time > upper_bound_time) {
      log_manager_->LogDiag(ProdDiagCode::OPSTATS_INVALID_SELECTION_CRITERIA);
      return absl::InvalidArgumentError(
          "Invalid selection criteria: start_time is after end_time.");
    }
    last_successful_contribution = criteria.last_successful_contribution();
  }

  FCP_ASSIGN_OR_RETURN(OpStatsSequence data,
                       op_stats_logger_->GetOpStatsDb()->Read());
  std::vector<OperationalStats> selected_data;
  if (last_successful_contribution) {
    // Selector specified last_successful_contribution, create a
    // last_successful_contribution iterator.
    std::optional<OperationalStats> last_successful_contribution_entry =
        GetLastSuccessfulContribution(data,
                                      op_stats_logger_->GetCurrentTaskName());
    if (last_successful_contribution_entry.has_value()) {
      selected_data.push_back(*last_successful_contribution_entry);
    }
  } else {
    selected_data = GetOperationalStatsForTimeRange(data, lower_bound_time,
                                                    upper_bound_time);
  }
  return std::make_unique<OpStatsExampleIterator>(
      std::move(selected_data),
      TimeUtil::TimestampToMilliseconds(data.earliest_trustworthy_time()));
}

}  // namespace opstats
}  // namespace client
}  // namespace fcp
