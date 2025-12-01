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
#ifndef FCP_CLIENT_ENGINE_COMMON_H_
#define FCP_CLIENT_ENGINE_COMMON_H_

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "fcp/client/engine/engine.pb.h"
#include "fcp/client/engine/example_iterator_factory.h"
#include "fcp/client/example_iterator_query_recorder.h"
#include "fcp/client/federated_protocol.h"
#include "fcp/client/opstats/opstats_logger.h"
#include "fcp/client/simple_task_environment.h"
#include "fcp/client/stats.h"
#include "fcp/protos/confidentialcompute/payload_metadata.pb.h"
#include "fcp/protos/data_type.pb.h"
#include "fcp/protos/federated_api.pb.h"
#include "fcp/protos/plan.pb.h"
#include "fcp/tensorflow/external_dataset.h"

namespace fcp {
namespace client {
namespace engine {

enum class PlanOutcome {
  kSuccess,
  // A TensorFlow error occurred.
  kTensorflowError,
  // Computation was interrupted.
  kInterrupted,
  // The input parameters are invalid.
  kInvalidArgument,
  // An example iterator error occurred.
  kExampleIteratorError,
};

struct FederatedComputeCheckpoint {
  absl::Cord payload;
  std::optional<fcp::confidentialcompute::PayloadMetadata> metadata;
};

// The result of a call to `SimplePlanEngine::RunPlan` or
// `TfLitePlanEngine::RunPlan`.
struct PlanResult {
  explicit PlanResult(PlanOutcome outcome, absl::Status status);

  // The outcome of the plan execution.
  PlanOutcome outcome;
  // The secagg tensors from the plan execution.
  absl::flat_hash_map<std::string, QuantizedTensor> secagg_tensor_map;
  // Only set if 'outcome' is 'kSuccess' and the federated compute wire format
  // is enabled, otherwise this is empty. Currently only supports a single
  // checkpoint.
  // TODO: b/422862369 - support multiple checkpoints.
  std::vector<FederatedComputeCheckpoint> federated_compute_checkpoints;
  // When the outcome is `kSuccess`, the status is ok. Otherwise, this status
  // contain the original error status which leads to the PlanOutcome.
  absl::Status original_status;
  ::fcp::client::ExampleStats example_stats;
  // Only set if the plan is an eligibility eval plan.
  absl::StatusOr<google::internal::federatedml::v2::TaskEligibilityInfo>
      task_eligibility_info;

  PlanResult(PlanResult&&) = default;
  PlanResult& operator=(PlanResult&&) = default;

  // Disallow copy and assign.
  PlanResult(const PlanResult&) = delete;
  PlanResult& operator=(const PlanResult&) = delete;
};

// Validates that the input tensors match what's inside the TensorflowSpec.
absl::Status ValidateTensorflowSpec(
    const google::internal::federated::plan::TensorflowSpec& tensorflow_spec,
    const absl::flat_hash_set<std::string>& expected_input_tensor_names_set,
    const std::vector<std::string>& output_names);

PhaseOutcome ConvertPlanOutcomeToPhaseOutcome(PlanOutcome plan_outcome);

absl::Status ConvertPlanOutcomeToStatus(engine::PlanOutcome outcome);

// Tracks whether any example iterator encountered an error during the
// computation (a single computation may use multiple iterators), either during
// creation of the iterator or during one of the iterations.
// This class is thread-safe.
class ExampleIteratorStatus {
 public:
  void SetStatus(absl::Status status) ABSL_LOCKS_EXCLUDED(mu_);
  absl::Status GetStatus() ABSL_LOCKS_EXCLUDED(mu_);

 private:
  absl::Status status_ ABSL_GUARDED_BY(mu_) = absl::OkStatus();
  mutable absl::Mutex mu_;
};

// Finds a suitable example iterator factory out of provided factories based on
// the provided selector.
ExampleIteratorFactory* FindExampleIteratorFactory(
    const google::internal::federated::plan::ExampleSelector& selector,
    std::vector<ExampleIteratorFactory*> example_iterator_factories);

// A class to iterate over a given example iterator.
class DatasetIterator : public ExternalDatasetIterator {
 public:
  DatasetIterator(std::unique_ptr<ExampleIterator> example_iterator,
                  opstats::OpStatsLogger* opstats_logger,
                  SingleExampleIteratorQueryRecorder* single_query_recorder,
                  std::atomic<int>* total_example_count,
                  std::atomic<int64_t>* total_example_size_bytes,
                  ExampleIteratorStatus* example_iterator_status,
                  const std::string& collection_uri, bool collect_stats);
  ~DatasetIterator() override;

  // Returns the next entry from the dataset.
  absl::StatusOr<std::string> GetNext() final;

 private:
  std::unique_ptr<ExampleIterator> example_iterator_
      ABSL_GUARDED_BY(iterator_lock_);
  opstats::OpStatsLogger* opstats_logger_;
  SingleExampleIteratorQueryRecorder* single_query_recorder_;
  absl::Time iterator_start_time_;
  // Example stats across all datasets.
  std::atomic<int>* total_example_count_;
  std::atomic<int64_t>* total_example_size_bytes_;
  ExampleIteratorStatus* example_iterator_status_;
  // Example stats only for this dataset.
  std::atomic<int> example_count_;
  std::atomic<int64_t> example_size_bytes_;
  const std::string collection_uri_;
  bool iterator_finished_ ABSL_GUARDED_BY(iterator_lock_);
  const bool collect_stats_;
  absl::Mutex iterator_lock_;
};

// Parse ExampleQuery and returns a map of (vector name) -> tuple(output name,
// vector spec).
absl::flat_hash_map<
    std::string,
    std::tuple<std::string, google::internal::federated::plan::
                                ExampleQuerySpec::OutputVectorSpec>>
GetOutputVectorSpecs(
    const google::internal::federated::plan::ExampleQuerySpec::ExampleQuery&
        example_query);
// Checks if the output vector data type matches the expected data type.
absl::Status CheckOutputVectorDataType(
    const google::internal::federated::plan::ExampleQuerySpec::OutputVectorSpec&
        output_vector_spec,
    const google::internal::federated::plan::DataType& expected_data_type);

}  // namespace engine
}  // namespace client
}  // namespace fcp

#endif  // FCP_CLIENT_ENGINE_COMMON_H_
