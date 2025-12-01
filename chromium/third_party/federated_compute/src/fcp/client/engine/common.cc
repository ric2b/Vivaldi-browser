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
#include "fcp/client/engine/common.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/clock.h"
#include "fcp/base/monitoring.h"
#include "fcp/client/engine/example_iterator_factory.h"
#include "fcp/client/example_iterator_query_recorder.h"
#include "fcp/client/opstats/opstats_logger.h"
#include "fcp/client/simple_task_environment.h"
#include "fcp/protos/data_type.pb.h"
#include "fcp/protos/plan.pb.h"
#include "tensorflow/core/protobuf/struct.pb.h"

namespace fcp {
namespace client {
namespace engine {

using ::google::internal::federated::plan::ExampleQuerySpec;
using ::google::internal::federated::plan::ExampleSelector;
using ::google::internal::federated::plan::TensorflowSpec;

PlanResult::PlanResult(PlanOutcome outcome, absl::Status status)
    : outcome(outcome), original_status(std::move(status)) {
  if (outcome == PlanOutcome::kSuccess) {
    FCP_CHECK(original_status.ok());
  }
}

absl::Status ValidateTensorflowSpec(
    const TensorflowSpec& tensorflow_spec,
    const absl::flat_hash_set<std::string>& expected_input_tensor_names_set,
    const std::vector<std::string>& output_names) {
  // Check that all inputs have corresponding TensorSpecProtos.
  if (expected_input_tensor_names_set.size() !=
      tensorflow_spec.input_tensor_specs_size()) {
    return absl::InvalidArgumentError(
        "Unexpected number of input_tensor_specs");
  }

  for (const tensorflow::TensorSpecProto& it :
       tensorflow_spec.input_tensor_specs()) {
    if (!expected_input_tensor_names_set.contains(it.name())) {
      return absl::InvalidArgumentError(absl::StrCat(
          "Missing expected TensorSpecProto for input ", it.name()));
    }
  }
  // Check that all outputs have corresponding TensorSpecProtos.
  absl::flat_hash_set<std::string> expected_output_tensor_names_set(
      output_names.begin(), output_names.end());
  if (expected_output_tensor_names_set.size() !=
      tensorflow_spec.output_tensor_specs_size()) {
    return absl::InvalidArgumentError(
        absl::StrCat("Unexpected number of output_tensor_specs: ",
                     expected_output_tensor_names_set.size(), " vs. ",
                     tensorflow_spec.output_tensor_specs_size()));
  }
  for (const tensorflow::TensorSpecProto& it :
       tensorflow_spec.output_tensor_specs()) {
    if (!expected_output_tensor_names_set.count(it.name())) {
      return absl::InvalidArgumentError(absl::StrCat(
          "Missing expected TensorSpecProto for output ", it.name()));
    }
  }

  return absl::OkStatus();
}

PhaseOutcome ConvertPlanOutcomeToPhaseOutcome(PlanOutcome plan_outcome) {
  switch (plan_outcome) {
    case PlanOutcome::kSuccess:
      return PhaseOutcome::COMPLETED;
    case PlanOutcome::kInterrupted:
      return PhaseOutcome::INTERRUPTED;
    case PlanOutcome::kTensorflowError:
    case PlanOutcome::kInvalidArgument:
    case PlanOutcome::kExampleIteratorError:
      return PhaseOutcome::ERROR;
  }
}

absl::Status ConvertPlanOutcomeToStatus(PlanOutcome outcome) {
  switch (outcome) {
    case PlanOutcome::kSuccess:
      return absl::OkStatus();
    case PlanOutcome::kTensorflowError:
    case PlanOutcome::kInvalidArgument:
    case PlanOutcome::kExampleIteratorError:
      return absl::InternalError("");
    case PlanOutcome::kInterrupted:
      return absl::CancelledError("");
  }
}

void ExampleIteratorStatus::SetStatus(absl::Status status) {
  absl::MutexLock lock(&mu_);
  // We ignores normal status such as ok and outOfRange to avoid running into a
  // race condition when an error happened, then an outofRange or ok status
  // returned in a different thread which overrides the error status.
  if (status.code() != absl::StatusCode::kOk &&
      status.code() != absl::StatusCode::kOutOfRange) {
    status_ = status;
  }
}

absl::Status ExampleIteratorStatus::GetStatus() {
  absl::MutexLock lock(&mu_);
  return status_;
}

ExampleIteratorFactory* FindExampleIteratorFactory(
    const ExampleSelector& selector,
    std::vector<ExampleIteratorFactory*> example_iterator_factories) {
  for (ExampleIteratorFactory* factory : example_iterator_factories) {
    if (factory->CanHandle(selector)) {
      return factory;
    }
  }
  return nullptr;
}

DatasetIterator::DatasetIterator(
    std::unique_ptr<ExampleIterator> example_iterator,
    opstats::OpStatsLogger* opstats_logger,
    SingleExampleIteratorQueryRecorder* single_query_recorder,
    std::atomic<int>* total_example_count,
    std::atomic<int64_t>* total_example_size_bytes,
    ExampleIteratorStatus* example_iterator_status,
    const std::string& collection_uri, bool collect_stats)
    : example_iterator_(std::move(example_iterator)),
      opstats_logger_(opstats_logger),
      single_query_recorder_(single_query_recorder),
      iterator_start_time_(absl::Now()),
      total_example_count_(total_example_count),
      total_example_size_bytes_(total_example_size_bytes),
      example_iterator_status_(example_iterator_status),
      example_count_(0),
      example_size_bytes_(0),
      collection_uri_(collection_uri),
      iterator_finished_(false),
      collect_stats_(collect_stats) {}

DatasetIterator::~DatasetIterator() {
  if (collect_stats_) {
    opstats_logger_->UpdateDatasetStats(collection_uri_, example_count_,
                                        example_size_bytes_);
  }
}

// Returns the next entry from the dataset.
absl::StatusOr<std::string> DatasetIterator::GetNext() {
  absl::MutexLock locked(&iterator_lock_);
  if (iterator_finished_) {
    // If we've reached the end of the iterator, always return OUT_OF_RANGE.
    return absl::OutOfRangeError("End of iterator reached");
  }
  absl::StatusOr<std::string> example = example_iterator_->Next();
  absl::StatusCode error_code = example.status().code();
  example_iterator_status_->SetStatus(example.status());
  if (error_code == absl::StatusCode::kOutOfRange) {
    example_iterator_->Close();
    iterator_finished_ = true;
  }
  if (example.ok()) {
    if (single_query_recorder_) {
      single_query_recorder_->Increment();
    }
    // If we're not forwarding an OUT_OF_RANGE to the caller, record example
    // stats for metrics logging.
    if (collect_stats_) {
      // TODO: b/184863488 - Consider reducing logic duplication in
      // cross-dataset and single-dataset example stat variables.
      *total_example_count_ += 1;
      *total_example_size_bytes_ += example->size();
      example_count_ += 1;
      example_size_bytes_ += example->size();
    }
  }
  return example;
}

// Returns a map of (vector name) -> tuple(output name, vector spec).
absl::flat_hash_map<std::string,
                    std::tuple<std::string, ExampleQuerySpec::OutputVectorSpec>>
GetOutputVectorSpecs(const ExampleQuerySpec::ExampleQuery& example_query) {
  absl::flat_hash_map<
      std::string, std::tuple<std::string, ExampleQuerySpec::OutputVectorSpec>>
      map;
  for (auto const& [output_name, output_vector_spec] :
       example_query.output_vector_specs()) {
    map[output_vector_spec.vector_name()] =
        std::make_tuple(output_name, output_vector_spec);
  }
  return map;
}

absl::Status CheckOutputVectorDataType(
    const ExampleQuerySpec::OutputVectorSpec& output_vector_spec,
    const google::internal::federated::plan::DataType& expected_data_type) {
  if (output_vector_spec.data_type() != expected_data_type) {
    return absl::FailedPreconditionError(
        "Unexpected data type in the example query");
  }
  return absl::OkStatus();
}

}  // namespace engine
}  // namespace client
}  // namespace fcp
