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
#include "fcp/client/engine/simple_plan_engine.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "google/protobuf/any.pb.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "fcp/base/monitoring.h"
#include "fcp/client/engine/common.h"
#include "fcp/client/engine/example_iterator_factory.h"
#include "fcp/client/engine/plan_engine_helpers.h"
#include "fcp/client/engine/tensorflow_utils.h"
#include "fcp/client/engine/tf_wrapper.h"
#include "fcp/client/example_iterator_query_recorder.h"
#include "fcp/client/interruptible_runner.h"
#include "fcp/client/log_manager.h"
#include "fcp/client/opstats/opstats_logger.h"
#include "fcp/tensorflow/host_object.h"
#include "tensorflow/core/framework/tensor.h"
#include "tensorflow/core/framework/tensor.pb.h"
#include "tensorflow/core/protobuf/struct.pb.h"

namespace fcp {
namespace client {
namespace engine {

using ::fcp::client::opstats::OpStatsLogger;
using ::google::internal::federated::plan::TensorflowSpec;

SimplePlanEngine::SimplePlanEngine(
    std::vector<ExampleIteratorFactory*> example_iterator_factories,
    std::function<bool()> should_abort, LogManager* log_manager,
    OpStatsLogger* opstats_logger,
    ExampleIteratorQueryRecorder* example_iterator_query_recorder,
    const InterruptibleRunner::TimingConfig* timing_config)
    : example_iterator_factories_(example_iterator_factories),
      should_abort_(should_abort),
      log_manager_(log_manager),
      opstats_logger_(opstats_logger),
      example_iterator_query_recorder_(example_iterator_query_recorder),
      timing_config_(timing_config) {}

PlanResult SimplePlanEngine::RunPlan(
    const TensorflowSpec& tensorflow_spec, const std::string& graph,
    const ::google::protobuf::Any& config_proto,
    std::unique_ptr<std::vector<std::pair<std::string, tensorflow::Tensor>>>
        inputs,
    const std::vector<std::string>& output_names,
    bool is_eligibility_eval_plan) {
  // Check that all inputs have corresponding TensorSpecProtos.
  absl::flat_hash_set<std::string> expected_input_tensor_names_set;
  for (const std::pair<std::string, tensorflow::Tensor>& input : *inputs) {
    expected_input_tensor_names_set.insert(input.first);
  }
  absl::Status validity_checks = ValidateTensorflowSpec(
      tensorflow_spec, expected_input_tensor_names_set, output_names);
  if (!validity_checks.ok()) {
    FCP_LOG(ERROR) << validity_checks.message();
    return PlanResult(PlanOutcome::kInvalidArgument,
                      std::move(validity_checks));
  }

  absl::StatusOr<std::unique_ptr<TensorFlowWrapper>> tf_wrapper_or =
      TensorFlowWrapper::Create(graph, config_proto, should_abort_,
                                *timing_config_, log_manager_);
  if (!tf_wrapper_or.ok()) {
    return PlanResult(PlanOutcome::kTensorflowError, tf_wrapper_or.status());
  }

  std::unique_ptr<TensorFlowWrapper> tf_wrapper =
      std::move(tf_wrapper_or.value());
  std::atomic<int> total_example_count = 0;
  std::atomic<int64_t> total_example_size_bytes = 0;
  ExampleIteratorStatus example_iterator_status;
  auto tf_result =
      RunPlanInternal(tf_wrapper.get(), tensorflow_spec, std::move(inputs),
                      output_names, &total_example_count,
                      &total_example_size_bytes, &example_iterator_status);
  FCP_CHECK(tf_wrapper->CloseAndRelease().ok());

  switch (tf_result.status().code()) {
    case absl::StatusCode::kOk: {
      PlanResult plan_result(PlanOutcome::kSuccess, absl::OkStatus());
      if (is_eligibility_eval_plan) {
        plan_result.task_eligibility_info =
            ParseEligibilityEvalPlanOutput(tf_result.value());
      } else {
        auto secagg_tensor_map =
            CreateQuantizedTensorMap(output_names, *tf_result, tensorflow_spec);
        if (!secagg_tensor_map.ok()) {
          return PlanResult(PlanOutcome::kTensorflowError,
                            secagg_tensor_map.status());
        }
        plan_result.secagg_tensor_map = std::move(*secagg_tensor_map);
      }
      plan_result.example_stats = {
          .example_count = total_example_count,
          .example_size_bytes = total_example_size_bytes};
      return plan_result;
    }
    case absl::StatusCode::kCancelled:
      return PlanResult(PlanOutcome::kInterrupted, tf_result.status());
    case absl::StatusCode::kInvalidArgument:
      return CreateComputationErrorPlanResult(
          example_iterator_status.GetStatus(), tf_result.status());
    default:
      FCP_LOG(FATAL) << "unexpected status code: " << tf_result.status().code();
  }
  // Unreachable, but clang doesn't get it.
  return PlanResult(PlanOutcome::kTensorflowError, absl::InternalError(""));
}

absl::StatusOr<std::vector<tensorflow::Tensor>>
SimplePlanEngine::RunPlanInternal(
    TensorFlowWrapper* tf_wrapper,
    const google::internal::federated::plan::TensorflowSpec& tensorflow_spec,
    std::unique_ptr<std::vector<std::pair<std::string, tensorflow::Tensor>>>
        inputs,
    const std::vector<std::string>& output_names,
    std::atomic<int>* total_example_count,
    std::atomic<int64_t>* total_example_size_bytes,
    ExampleIteratorStatus* example_iterator_status) {
  // Populate input tensor vector
  // AddDatasetTokenToInputs first registers a DatasetProvider with the global
  // ExternalDatasetProviderRegistry and then returns a HostObjectRegistration
  // object. Hold onto the HostObjectRegistration object since it de-registers
  // upon destruction.
  HostObjectRegistration host_registration = AddDatasetTokenToInputs(
      example_iterator_factories_, opstats_logger_,
      example_iterator_query_recorder_, inputs.get(),
      tensorflow_spec.dataset_token_tensor_name(), total_example_count,
      total_example_size_bytes, example_iterator_status);

  std::vector<std::string> target_names;
  for (const std::string& target_node_name :
       tensorflow_spec.target_node_names()) {
    target_names.push_back(target_node_name);
  }
  if (!tensorflow_spec.constant_inputs().empty()) {
    // If the server-side constant inputs message is provided, copy over these
    // values to the set of input tensors.
    for (const auto& [name, tensor_proto] : tensorflow_spec.constant_inputs()) {
      tensorflow::Tensor input_tensor;
      if (!input_tensor.FromProto(tensor_proto)) {
        return absl::InvalidArgumentError(
            absl::StrCat("unable to convert constant_input to tensor: %s",
                         tensor_proto.DebugString()));
      }
      inputs->push_back({name, std::move(input_tensor)});
    }
  }

  FCP_ASSIGN_OR_RETURN(
      auto result,
      RunTensorFlowInternal(tf_wrapper, *inputs, output_names, target_names));
  return result;
}

absl::StatusOr<std::vector<tensorflow::Tensor>>
SimplePlanEngine::RunTensorFlowInternal(
    TensorFlowWrapper* tf_wrapper,
    const std::vector<std::pair<std::string, tensorflow::Tensor>>& inputs,
    const std::vector<std::string>& output_tensor_names,
    const std::vector<std::string>& target_node_names) {
  std::vector<tensorflow::Tensor> outputs;
  absl::Status status =
      tf_wrapper->Run(inputs, output_tensor_names, target_node_names, &outputs);
  switch (status.code()) {
    case absl::StatusCode::kCancelled:
    case absl::StatusCode::kInvalidArgument:
      return status;
    case absl::StatusCode::kOutOfRange:
    case absl::StatusCode::kOk:
      break;
    default:
      FCP_CHECK_STATUS(status);
  }
  return outputs;
}

}  // namespace engine
}  // namespace client
}  // namespace fcp
