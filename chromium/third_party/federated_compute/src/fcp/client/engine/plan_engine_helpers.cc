/*
 * Copyright 2020 Google LLC
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
#include "fcp/client/engine/plan_engine_helpers.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/mutex.h"
#include "fcp/client/diag_codes.pb.h"
#include "fcp/client/engine/common.h"
#include "fcp/client/engine/example_iterator_factory.h"
#include "fcp/client/example_iterator_query_recorder.h"
#include "fcp/client/opstats/opstats_logger.h"
#include "fcp/client/simple_task_environment.h"
#include "fcp/protos/plan.pb.h"
#include "fcp/tensorflow/external_dataset.h"
#include "fcp/tensorflow/host_object.h"
#include "tensorflow/core/framework/tensor.h"
#include "tensorflow/core/platform/tstring.h"

namespace fcp {
namespace client {
namespace engine {
namespace {

using ::fcp::client::opstats::OpStatsLogger;
using ::google::internal::federated::plan::ExampleSelector;

/** An iterator that forwards the failing status from the external dataset to
 * TensorFlow. */
class FailingDatasetIterator : public ExternalDatasetIterator {
 public:
  explicit FailingDatasetIterator(absl::Status status) : status_(status) {}

  absl::StatusOr<std::string> GetNext() final { return status_; }

 private:
  const absl::Status status_;
};

class TrainingDatasetProvider
    : public ExternalDatasetProvider::UsingProtoSelector<ExampleSelector> {
 public:
  TrainingDatasetProvider(
      std::vector<ExampleIteratorFactory*> example_iterator_factories,
      OpStatsLogger* opstats_logger,
      ExampleIteratorQueryRecorder* example_iterator_query_recorder,
      std::atomic<int>* total_example_count,
      std::atomic<int64_t>* total_example_size_bytes,
      ExampleIteratorStatus* example_iterator_status)
      : example_iterator_factories_(example_iterator_factories),
        opstats_logger_(opstats_logger),
        example_iterator_query_recorder_(example_iterator_query_recorder),
        total_example_count_(total_example_count),
        total_example_size_bytes_(total_example_size_bytes),
        example_iterator_status_(example_iterator_status) {}

  absl::StatusOr<std::unique_ptr<ExternalDataset>> MakeDataset(
      ExampleSelector selector) final {
    return ExternalDataset::FromFunction(
        [example_iterator_factories = example_iterator_factories_,
         opstats_logger = opstats_logger_,
         example_iterator_query_recorder = example_iterator_query_recorder_,
         selector, total_example_count = total_example_count_,
         total_example_size_bytes = total_example_size_bytes_,
         example_iterator_status = example_iterator_status_]()
            -> std::unique_ptr<ExternalDatasetIterator> {
          ExampleIteratorFactory* example_iterator_factory =
              FindExampleIteratorFactory(selector, example_iterator_factories);
          // The DatasetOp requires a valid iterator at this stage so return an
          // empty iterator if there was an error.
          if (example_iterator_factory == nullptr) {
            absl::Status error(
                absl::StatusCode::kInternal,
                "Could not find suitable ExampleIteratorFactory");
            example_iterator_status->SetStatus(error);
            return std::make_unique<FailingDatasetIterator>(error);
          }
          absl::StatusOr<std::unique_ptr<ExampleIterator>> example_iterator =
              example_iterator_factory->CreateExampleIterator(selector);
          if (!example_iterator.ok()) {
            example_iterator_status->SetStatus(example_iterator.status());
            return std::make_unique<FailingDatasetIterator>(
                example_iterator.status());
          }
          SingleExampleIteratorQueryRecorder* single_query_recorder = nullptr;
          if (example_iterator_query_recorder) {
            single_query_recorder =
                example_iterator_query_recorder->RecordQuery(selector);
          }
          return std::make_unique<DatasetIterator>(
              std::move(*example_iterator), opstats_logger,
              single_query_recorder, total_example_count,
              total_example_size_bytes, example_iterator_status,
              selector.collection_uri(),
              /*collect_stats=*/
              example_iterator_factory->ShouldCollectStats());
        });
  }

 private:
  std::vector<ExampleIteratorFactory*> example_iterator_factories_;
  OpStatsLogger* opstats_logger_;
  ExampleIteratorQueryRecorder* example_iterator_query_recorder_;
  std::atomic<int>* total_example_count_;
  std::atomic<int64_t>* total_example_size_bytes_;
  ExampleIteratorStatus* example_iterator_status_;
};

}  // namespace

HostObjectRegistration AddDatasetTokenToInputs(
    std::vector<ExampleIteratorFactory*> example_iterator_factories,
    OpStatsLogger* opstats_logger,
    ExampleIteratorQueryRecorder* example_iterator_query_recorder,
    std::vector<std::pair<std::string, tensorflow::Tensor>>* inputs,
    const std::string& dataset_token_tensor_name,
    std::atomic<int>* total_example_count,
    std::atomic<int64_t>* total_example_size_bytes,
    ExampleIteratorStatus* example_iterator_status) {
  // Register the TrainingDatasetProvider with the global
  // ExternalDatasetProviderRegistry.
  auto host_registration = fcp::ExternalDatasetProviderRegistry::Register(
      std::make_shared<TrainingDatasetProvider>(
          example_iterator_factories, opstats_logger,
          example_iterator_query_recorder, total_example_count,
          total_example_size_bytes, example_iterator_status));
  // Pack the token returned from registering the provider into a string
  // tensor. TensorFlow will use that token via the ExternalDatasetOp to create
  // datasets and iterators.
  tensorflow::Tensor token_scalar(std::string{});
  token_scalar.scalar<tensorflow::tstring>()() =
      host_registration.token().ToString();
  std::pair<std::string, tensorflow::Tensor> token_pair(
      dataset_token_tensor_name, token_scalar);
  inputs->emplace_back(token_pair);
  return host_registration;
}

HostObjectRegistration AddDatasetTokenToInputsForTfLite(
    std::vector<ExampleIteratorFactory*> example_iterator_factories,
    OpStatsLogger* opstats_logger,
    ExampleIteratorQueryRecorder* example_iterator_query_recorder,
    absl::flat_hash_map<std::string, std::string>* inputs,
    const std::string& dataset_token_tensor_name,
    std::atomic<int>* total_example_count,
    std::atomic<int64_t>* total_example_size_bytes,
    ExampleIteratorStatus* example_iterator_status) {
  // Registers the TrainingDatasetProvider with the global
  // ExternalDatasetProviderRegistry.
  auto host_registration = fcp::ExternalDatasetProviderRegistry::Register(
      std::make_shared<TrainingDatasetProvider>(
          example_iterator_factories, opstats_logger,
          example_iterator_query_recorder, total_example_count,
          total_example_size_bytes, example_iterator_status));
  // Adds the token returned from registering the provider to the map of inputs.
  // TfLite will use that token via the ExternalDatasetOp to create
  // datasets and iterators.
  (*inputs)[dataset_token_tensor_name] = host_registration.token().ToString();
  return host_registration;
}

PlanResult CreateComputationErrorPlanResult(
    absl::Status example_iterator_status,
    absl::Status computation_error_status) {
  switch (example_iterator_status.code()) {
    case absl::StatusCode::kOk:
    case absl::StatusCode::kOutOfRange:
      // Either example iterators are working fine or we don't know the status
      // of the example iterators. In this case, we'll use the error status
      // returned from TensorFlow.
      return PlanResult(PlanOutcome::kTensorflowError,
                        computation_error_status);
    case absl::StatusCode::kCancelled:
      // Example iterator got interrupted.
      return PlanResult(PlanOutcome::kInterrupted, example_iterator_status);
    default:
      // All other Example iterator errors.
      return PlanResult(PlanOutcome::kExampleIteratorError,
                        example_iterator_status);
  }
}

}  // namespace engine
}  // namespace client
}  // namespace fcp
