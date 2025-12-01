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
#include "fcp/client/engine/tflite_plan_engine.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "fcp/base/monitoring.h"
#include "fcp/client/diag_codes.pb.h"
#include "fcp/client/engine/common.h"
#include "fcp/client/engine/plan_engine_helpers.h"
#include "fcp/client/engine/tensorflow_utils.h"
#include "fcp/client/engine/tflite_wrapper.h"
#include "fcp/client/flags.h"
#include "fcp/protos/plan.pb.h"
#include "fcp/tensorflow/host_object.h"
#include "tensorflow/core/framework/tensor.h"
#include "tensorflow/core/framework/tensor.pb.h"
#include "tensorflow/core/platform/tstring.h"
#include "tensorflow/core/protobuf/struct.pb.h"

namespace fcp {
namespace client {
namespace engine {

using ::google::internal::federated::plan::TensorflowSpec;

namespace {

PlanResult CreatePlanResultFromOutput(
    absl::StatusOr<OutputTensors> output, std::atomic<int>* total_example_count,
    std::atomic<int64_t>* total_example_size_bytes,
    absl::Status example_iterator_status, bool is_eligibility_eval_plan,
    const TensorflowSpec& tensorflow_spec) {
  switch (output.status().code()) {
    case absl::StatusCode::kOk: {
      PlanResult plan_result(PlanOutcome::kSuccess, absl::OkStatus());
      if (is_eligibility_eval_plan) {
        plan_result.task_eligibility_info =
            ParseEligibilityEvalPlanOutput(output->output_tensors);
      } else {
        auto secagg_tensor_map =
            CreateQuantizedTensorMap(output->output_tensor_names,
                                     output->output_tensors, tensorflow_spec);
        if (!secagg_tensor_map.ok()) {
          return PlanResult(PlanOutcome::kTensorflowError,
                            secagg_tensor_map.status());
        }
        plan_result.secagg_tensor_map = std::move(*secagg_tensor_map);
      }
      plan_result.example_stats = {
          .example_count = *total_example_count,
          .example_size_bytes = *total_example_size_bytes};
      return plan_result;
    }
    case absl::StatusCode::kCancelled:
      return PlanResult(PlanOutcome::kInterrupted, std::move(output.status()));
    case absl::StatusCode::kInvalidArgument:
      return CreateComputationErrorPlanResult(example_iterator_status,
                                              output.status());
    default:
      FCP_LOG(FATAL) << "unexpected status code: " << output.status().code();
  }
  // Unreachable code.
  return PlanResult(PlanOutcome::kTensorflowError, absl::InternalError(""));
}

TfLiteInterpreterOptions CreateOptions(const Flags& flags) {
  return TfLiteInterpreterOptions{
      .ensure_dynamic_tensors_are_released =
          flags.ensure_dynamic_tensors_are_released(),
      .large_tensor_threshold_for_dynamic_allocation =
          flags.large_tensor_threshold_for_dynamic_allocation(),
      .disable_delegate_clustering = flags.disable_tflite_delegate_clustering(),
      .use_builtin_op_resolver_with_default_delegates =
          flags.tflite_use_builtin_op_resolver_with_default_delegates()};
}
}  // namespace

PlanResult TfLitePlanEngine::RunPlan(
    const TensorflowSpec& tensorflow_spec, const std::string& model,
    std::unique_ptr<absl::flat_hash_map<std::string, std::string>> inputs,
    const std::vector<std::string>& output_names,
    bool is_eligibility_eval_plan) {
  log_manager_->LogDiag(ProdDiagCode::BACKGROUND_TRAINING_TFLITE_ENGINE_USED);
  // Check that all inputs have corresponding TensorSpecProtos.
  absl::flat_hash_set<std::string> expected_input_tensor_names_set;
  for (auto it = inputs->begin(); it != inputs->end(); it++) {
    expected_input_tensor_names_set.insert(it->first);
  }
  absl::Status validity_checks = ValidateTensorflowSpec(
      tensorflow_spec, expected_input_tensor_names_set, output_names);
  if (!validity_checks.ok()) {
    FCP_LOG(ERROR) << validity_checks.message();
    return PlanResult(PlanOutcome::kInvalidArgument,
                      std::move(validity_checks));
  }
  std::atomic<int> total_example_count = 0;
  std::atomic<int64_t> total_example_size_bytes = 0;
  ExampleIteratorStatus example_iterator_status;
  HostObjectRegistration host_registration = AddDatasetTokenToInputsForTfLite(
      example_iterator_factories_, opstats_logger_,
      example_iterator_query_recorder_, inputs.get(),
      tensorflow_spec.dataset_token_tensor_name(), &total_example_count,
      &total_example_size_bytes, &example_iterator_status);
  // If the constant inputs are provided and the flag is enabled, add these to
  // the map of TFLite inputs.
  if (!tensorflow_spec.constant_inputs().empty()) {
    for (const auto& [name, tensor_proto] : tensorflow_spec.constant_inputs()) {
      tensorflow::Tensor input_tensor;
      if (!input_tensor.FromProto(tensor_proto)) {
        FCP_LOG(ERROR) << "unable to convert constant_input to tensor: "
                       << tensor_proto.DebugString();
        return PlanResult(
            PlanOutcome::kInvalidArgument,
            absl::InternalError("Unable to convert constant_input to tensor"));
      }
      // Convert Tensor to TFLite representation and add this as a string to
      // inputs.
      if (input_tensor.dtype() == tensorflow::DT_STRING) {
        tensorflow::tstring str_data =
            input_tensor.scalar<tensorflow::tstring>()();
        inputs->insert({name, std::string(str_data.data(), str_data.size())});
      } else {
        FCP_LOG(ERROR) << "Constant input tensor is not a string tensor. "
                          "Currently only string tensors are supported.";
        return PlanResult(
            PlanOutcome::kInvalidArgument,
            absl::InternalError("Only string tensors are supported"));
      }
    }
  }
  absl::StatusOr<OutputTensors> output = RunTfLiteModelThreadSafe(
      model, should_abort_, *timing_config_, log_manager_, std::move(inputs),
      output_names, CreateOptions(flags_), flags_.num_threads_for_tflite());
  PlanResult plan_result = CreatePlanResultFromOutput(
      std::move(output), &total_example_count, &total_example_size_bytes,
      example_iterator_status.GetStatus(), is_eligibility_eval_plan,
      tensorflow_spec);
  return plan_result;
}

}  // namespace engine
}  // namespace client
}  // namespace fcp
