/*
 * Copyright 2019 Google LLC
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
#ifndef FCP_CLIENT_ENGINE_PLAN_ENGINE_HELPERS_H_
#define FCP_CLIENT_ENGINE_PLAN_ENGINE_HELPERS_H_

#include <atomic>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/optimization.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "fcp/client/engine/common.h"
#include "fcp/client/engine/example_iterator_factory.h"
#include "fcp/client/example_iterator_query_recorder.h"
#include "fcp/client/opstats/opstats_logger.h"
#include "fcp/tensorflow/host_object.h"
#include "tensorflow/core/framework/tensor.h"

// On Error Handling
// Calls in the engine are assumed to either
// 1. be successful (Status::OK)
// 2. fail with an "expected" error -> handle gracefully - log error, tell the
//    environment (via finish), return
// 3. encounter "unexpected" errors; when originating inside the engine or in
//    native code in the environment, or from java, crash.
// While this type of tristate error handling is easy in Java (success, checked,
// unchecked exceptions), it isn't in C++, hence we adopt the following
// convention for control flow/error handling inside the engine:
// - all functions in the plan engine downstream of runPhase() that can fail
//   must return a Status with one of the following codes: INTERNAL_ERROR,
//   CANCELLED, INVALID_ARGUMENT, OK. Only on OK will normal execution continue,
//   otherwise return up to the top level (runPhase). Once at the top level,
//   those error codes will be handled as follows:
//   a) CANCELLED -> report INTERRUPTED to env
//   b) INTERNAL_ERROR/INVALID_ARGUMENT -> report ERROR to env
//   c) OK -> report COMPLETED to env
//   For all status codes, the TaskRetry returned from the env is returned.
// - utility functions outside of the engine will also use Status/StatusOr, but
//   may use other error codes (e.g. the TensorFlowWrapper or ExampleIterator
//   use OUT_OF_RANGE).
// Return error handling is beautiful, I use this macro:
// #1: FCP_ENGINE_RETURN_IF_ERROR(...): Return if the Status code is not OK,
// else continue.

namespace fcp {
namespace client {
namespace engine {
namespace internal {
inline absl::Status AsStatus(absl::Status status) { return status; }
}  // namespace internal

// Macro to return the provided Status (or Status contained in StatusOr) if a
// call to ok() fails.
#define FCP_ENGINE_RETURN_IF_ERROR(status_or_statusor_expr)                 \
  do {                                                                      \
    const absl::Status __status =                                           \
        ::fcp::client::engine::internal::AsStatus(status_or_statusor_expr); \
    if (ABSL_PREDICT_FALSE(__status.code() != absl::StatusCode::kOk)) {     \
      return __status;                                                      \
    }                                                                       \
  } while (0)

// Sets up a ExternalDatasetProvider that is registered with the global
// HostObjectRegistry. Adds a tensor representing the HostObjectRegistration
// token to the input tensors with the provided dataset_token_tensor_name key.
//
// For each example query issued by the plan at runtime, the given
// `example_iterator_factories` parameter will be iterated and the first
// iterator factory that can handle the given query will be used to create the
// example iterator to handle that query.
HostObjectRegistration AddDatasetTokenToInputs(
    std::vector<ExampleIteratorFactory*> example_iterator_factories,
    ::fcp::client::opstats::OpStatsLogger* opstats_logger,
    ExampleIteratorQueryRecorder* example_iterator_query_recorder,
    std::vector<std::pair<std::string, tensorflow::Tensor>>* inputs,
    const std::string& dataset_token_tensor_name,
    std::atomic<int>* total_example_count,
    std::atomic<int64_t>* total_example_size_bytes,
    ExampleIteratorStatus* example_iterator_status);

// Sets up an ExternalDatasetProvider that is registered with the global
// HostObjectRegistry. Adds a string representing the HostObjectRegistration
// token to the map of input tensor name and values with the provided
// dataset_token_tensor_name key.
//
// For each example query issued by the plan at runtime, the given
// `example_iterator_factories` parameter will be iterated and the first
// iterator factory that can handle the given query will be used to create the
// example iterator to handle that query.
HostObjectRegistration AddDatasetTokenToInputsForTfLite(
    std::vector<ExampleIteratorFactory*> example_iterator_factories,
    ::fcp::client::opstats::OpStatsLogger* opstats_logger,
    ExampleIteratorQueryRecorder* example_iterator_query_recorder,
    absl::flat_hash_map<std::string, std::string>* inputs,
    const std::string& dataset_token_tensor_name,
    std::atomic<int>* total_example_count,
    std::atomic<int64_t>* total_example_size_bytes,
    ExampleIteratorStatus* example_iterator_status);

// Utility for creating a PlanResult when an `INVALID_ARGUMENT` TensorFlow error
// was encountered, disambiguating between generic TF errors and TF errors that
// were likely root-caused by an earlier example iterator error.
PlanResult CreateComputationErrorPlanResult(
    absl::Status example_iterator_status,
    absl::Status computation_error_status);

}  // namespace engine
}  // namespace client
}  // namespace fcp

#endif  // FCP_CLIENT_ENGINE_PLAN_ENGINE_HELPERS_H_
