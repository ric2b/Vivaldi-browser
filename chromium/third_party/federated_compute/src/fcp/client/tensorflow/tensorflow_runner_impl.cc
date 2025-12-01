/*
 * Copyright 2024 Google LLC
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

#include "fcp/client/tensorflow/tensorflow_runner_impl.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "fcp/base/monitoring.h"
#include "fcp/client/engine/common.h"
#include "fcp/client/engine/example_iterator_factory.h"
#include "fcp/client/engine/tflite_plan_engine.h"
#include "fcp/client/runner_common.h"

#ifdef FCP_CLIENT_SUPPORT_TFMOBILE
#include "fcp/client/engine/simple_plan_engine.h"
#endif

#include "fcp/client/example_iterator_query_recorder.h"
#include "fcp/client/example_query_result.pb.h"
#include "fcp/client/flags.h"
#include "fcp/client/interruptible_runner.h"
#include "fcp/client/log_manager.h"
#include "fcp/client/opstats/opstats_logger.h"
#include "fcp/protos/data_type.pb.h"
#include "fcp/protos/plan.pb.h"
#include "tensorflow/core/framework/tensor.h"
#include "tensorflow/core/framework/tensor_shape.h"
#include "tensorflow/core/framework/tensor_slice.h"
#include "tensorflow/core/platform/tstring.h"
#include "tensorflow/core/protobuf/struct.pb.h"
#include "tensorflow/core/util/tensor_slice_writer.h"

namespace fcp::client {

using TfLiteInputs = absl::flat_hash_map<std::string, std::string>;

using ::fcp::client::InterruptibleRunner;
using ::google::internal::federated::plan::ClientOnlyPlan;
using ::google::internal::federated::plan::DataType;
using ::google::internal::federated::plan::ExampleQuerySpec;
using ::google::internal::federated::plan::FederatedComputeEligibilityIORouter;
using ::google::internal::federated::plan::FederatedComputeIORouter;
using ::google::internal::federated::plan::TensorflowSpec;

namespace {

#ifdef FCP_CLIENT_SUPPORT_TFMOBILE
std::unique_ptr<std::vector<std::pair<std::string, tensorflow::Tensor>>>
ConstructInputsForEligibilityEvalPlan(
    const FederatedComputeEligibilityIORouter& io_router,
    const std::string& checkpoint_input_filename) {
  auto inputs = std::make_unique<
      std::vector<std::pair<std::string, tensorflow::Tensor>>>();
  if (!io_router.input_filepath_tensor_name().empty()) {
    tensorflow::Tensor input_filepath(tensorflow::DT_STRING, {});
    input_filepath.scalar<tensorflow::tstring>()() = checkpoint_input_filename;
    inputs->push_back({io_router.input_filepath_tensor_name(), input_filepath});
  }
  return inputs;
}
#endif

std::unique_ptr<TfLiteInputs> ConstructTfLiteInputsForEligibilityEvalPlan(
    const FederatedComputeEligibilityIORouter& io_router,
    const std::string& checkpoint_input_filename) {
  auto inputs = std::make_unique<TfLiteInputs>();
  if (!io_router.input_filepath_tensor_name().empty()) {
    (*inputs)[io_router.input_filepath_tensor_name()] =
        checkpoint_input_filename;
  }
  return inputs;
}

#ifdef FCP_CLIENT_SUPPORT_TFMOBILE
std::unique_ptr<std::vector<std::pair<std::string, tensorflow::Tensor>>>
ConstructInputsForTensorflowSpecPlan(
    const FederatedComputeIORouter& io_router,
    const std::string& checkpoint_input_filename,
    const std::string& checkpoint_output_filename) {
  auto inputs = std::make_unique<
      std::vector<std::pair<std::string, tensorflow::Tensor>>>();
  if (!io_router.input_filepath_tensor_name().empty()) {
    tensorflow::Tensor input_filepath(tensorflow::DT_STRING, {});
    input_filepath.scalar<tensorflow::tstring>()() = checkpoint_input_filename;
    inputs->push_back({io_router.input_filepath_tensor_name(), input_filepath});
  }

  if (!io_router.output_filepath_tensor_name().empty()) {
    tensorflow::Tensor output_filepath(tensorflow::DT_STRING, {});
    output_filepath.scalar<tensorflow::tstring>()() =
        checkpoint_output_filename;
    inputs->push_back(
        {io_router.output_filepath_tensor_name(), output_filepath});
  }

  return inputs;
}
#endif

std::unique_ptr<TfLiteInputs> ConstructTFLiteInputsForTensorflowSpecPlan(
    const FederatedComputeIORouter& io_router,
    const std::string& checkpoint_input_filename,
    const std::string& checkpoint_output_filename) {
  auto inputs = std::make_unique<TfLiteInputs>();
  if (!io_router.input_filepath_tensor_name().empty()) {
    (*inputs)[io_router.input_filepath_tensor_name()] =
        checkpoint_input_filename;
  }

  if (!io_router.output_filepath_tensor_name().empty()) {
    (*inputs)[io_router.output_filepath_tensor_name()] =
        checkpoint_output_filename;
  }

  return inputs;
}

absl::StatusOr<std::vector<std::string>> ConstructOutputsWithDeterministicOrder(
    const TensorflowSpec& tensorflow_spec,
    const FederatedComputeIORouter& io_router) {
  std::vector<std::string> output_names;
  // The order of output tensor names should match the order in TensorflowSpec.
  for (const auto& output_tensor_spec : tensorflow_spec.output_tensor_specs()) {
    const std::string& tensor_name = output_tensor_spec.name();
    if (!io_router.aggregations().contains(tensor_name) ||
        !io_router.aggregations().at(tensor_name).has_secure_aggregation()) {
      return absl::InvalidArgumentError(
          "Output tensor is missing in AggregationConfig, or has unsupported "
          "aggregation type.");
    }
    output_names.push_back(tensor_name);
  }

  return output_names;
}

// Writes an one-dimensional tensor using the slice writer.
template <typename T>
absl::Status WriteSlice(tensorflow::checkpoint::TensorSliceWriter& slice_writer,
                        const std::string& name, const int64_t size,
                        const T* data) {
  tensorflow::TensorShape shape;
  shape.AddDim(size);
  tensorflow::TensorSlice slice(shape.dims());
  return slice_writer.Add(name, shape, slice, data);
}

}  // namespace

engine::PlanResult
TensorflowRunnerImpl::RunEligibilityEvalPlanWithTensorflowSpec(
    std::vector<engine::ExampleIteratorFactory*> example_iterator_factories,
    std::function<bool()> should_abort, LogManager* log_manager,
    opstats::OpStatsLogger* opstats_logger, const Flags* flags,
    const ClientOnlyPlan& client_plan,
    const std::string& checkpoint_input_filename,
    const InterruptibleRunner::TimingConfig& timing_config,
    absl::Time run_plan_start_time, absl::Time reference_time) {
  // Check that this is a TensorflowSpec-based plan for federated eligibility
  // computation.
  if (!client_plan.phase().has_tensorflow_spec() ||
      !client_plan.phase().has_federated_compute_eligibility()) {
    return engine::PlanResult(
        engine::PlanOutcome::kInvalidArgument,
        absl::InvalidArgumentError("Invalid eligibility eval plan"));
  }
  const FederatedComputeEligibilityIORouter& io_router =
      client_plan.phase().federated_compute_eligibility();

  std::vector<std::string> output_names = {
      io_router.task_eligibility_info_tensor_name()};

  const bool tflite_model_included = !client_plan.tflite_graph().empty();
  if (tflite_model_included) {
    log_manager->LogDiag(
        ProdDiagCode::BACKGROUND_TRAINING_TFLITE_MODEL_INCLUDED);
  }
  if (flags->use_tflite_training() && tflite_model_included) {
    std::unique_ptr<TfLiteInputs> tflite_inputs =
        ConstructTfLiteInputsForEligibilityEvalPlan(io_router,
                                                    checkpoint_input_filename);
    engine::TfLitePlanEngine plan_engine(
        example_iterator_factories, should_abort, log_manager, opstats_logger,
        flags, /*example_iterator_query_recorder=*/nullptr, &timing_config);
    return plan_engine.RunPlan(client_plan.phase().tensorflow_spec(),
                               client_plan.tflite_graph(),
                               std::move(tflite_inputs), output_names,
                               /*is_eligibility_eval_plan=*/true);
  }

#ifdef FCP_CLIENT_SUPPORT_TFMOBILE
  // Construct input tensors and output tensor names based on the values in the
  // FederatedComputeEligibilityIORouter message.
  auto inputs = ConstructInputsForEligibilityEvalPlan(
      io_router, checkpoint_input_filename);
  // Run plan and get a set of output tensors back.
  engine::SimplePlanEngine plan_engine(
      example_iterator_factories, should_abort, log_manager, opstats_logger,
      /*example_iterator_query_recorder=*/nullptr, &timing_config);
  return plan_engine.RunPlan(
      client_plan.phase().tensorflow_spec(), client_plan.graph(),
      client_plan.tensorflow_config_proto(), std::move(inputs), output_names,
      /*is_eligibility_eval_plan=*/true);
#else
  return engine::PlanResult(
      engine::PlanOutcome::kTensorflowError,
      absl::InternalError("No eligibility eval plan engine enabled"));
#endif
}

PlanResultAndCheckpointFile TensorflowRunnerImpl::RunPlanWithTensorflowSpec(
    std::vector<engine::ExampleIteratorFactory*> example_iterator_factories,
    std::function<bool()> should_abort, LogManager* log_manager,
    opstats::OpStatsLogger* opstats_logger, const Flags* flags,
    ExampleIteratorQueryRecorder* example_iterator_query_recorder,
    const ClientOnlyPlan& client_plan,
    const std::string& checkpoint_input_filename,
    const std::string& checkpoint_output_filename,
    const fcp::client::InterruptibleRunner::TimingConfig& timing_config) {
  if (!client_plan.phase().has_tensorflow_spec()) {
    return PlanResultAndCheckpointFile(engine::PlanResult(
        engine::PlanOutcome::kInvalidArgument,
        absl::InvalidArgumentError("Plan must include TensorflowSpec.")));
  }
  if (!client_plan.phase().has_federated_compute()) {
    return PlanResultAndCheckpointFile(engine::PlanResult(
        engine::PlanOutcome::kInvalidArgument,
        absl::InvalidArgumentError("Invalid TensorflowSpec-based plan")));
  }

  // Get the output tensor names.
  absl::StatusOr<std::vector<std::string>> output_names;
  output_names = ConstructOutputsWithDeterministicOrder(
      client_plan.phase().tensorflow_spec(),
      client_plan.phase().federated_compute());
  if (!output_names.ok()) {
    return PlanResultAndCheckpointFile(engine::PlanResult(
        engine::PlanOutcome::kInvalidArgument, output_names.status()));
  }

  const bool tflite_model_included = !client_plan.tflite_graph().empty();
  if (tflite_model_included) {
    log_manager->LogDiag(
        ProdDiagCode::BACKGROUND_TRAINING_TFLITE_MODEL_INCLUDED);
  }
  // Run plan and get a set of output tensors back.
  if (flags->use_tflite_training() && tflite_model_included) {
    std::unique_ptr<TfLiteInputs> tflite_inputs =
        ConstructTFLiteInputsForTensorflowSpecPlan(
            client_plan.phase().federated_compute(), checkpoint_input_filename,
            checkpoint_output_filename);
    engine::TfLitePlanEngine plan_engine(
        example_iterator_factories, should_abort, log_manager, opstats_logger,
        flags, example_iterator_query_recorder, &timing_config);
    engine::PlanResult plan_result = plan_engine.RunPlan(
        client_plan.phase().tensorflow_spec(), client_plan.tflite_graph(),
        std::move(tflite_inputs), *output_names,
        /*is_eligibility_eval_plan=*/false);
    PlanResultAndCheckpointFile result(std::move(plan_result));
    result.checkpoint_filename = checkpoint_output_filename;

    return result;
  }

#ifdef FCP_CLIENT_SUPPORT_TFMOBILE
  // Construct input tensors based on the values in the
  // FederatedComputeIORouter message and create a temporary file for the output
  // checkpoint if needed.
  auto inputs = ConstructInputsForTensorflowSpecPlan(
      client_plan.phase().federated_compute(), checkpoint_input_filename,
      checkpoint_output_filename);
  engine::SimplePlanEngine plan_engine(
      example_iterator_factories, should_abort, log_manager, opstats_logger,
      example_iterator_query_recorder, &timing_config);
  engine::PlanResult plan_result = plan_engine.RunPlan(
      client_plan.phase().tensorflow_spec(), client_plan.graph(),
      client_plan.tensorflow_config_proto(), std::move(inputs), *output_names,
      /*is_eligibility_eval_plan=*/false);

  PlanResultAndCheckpointFile result(std::move(plan_result));
  result.checkpoint_filename = checkpoint_output_filename;

  return result;
#else
  return PlanResultAndCheckpointFile(
      engine::PlanResult(engine::PlanOutcome::kTensorflowError,
                         absl::InternalError("No plan engine enabled")));
#endif
}

// Writes example query results into a checkpoint.
absl::Status TensorflowRunnerImpl::WriteTFV1Checkpoint(
    const std::string& output_checkpoint_filename,
    const std::vector<std::pair<ExampleQuerySpec::ExampleQuery,
                                ExampleQueryResult>>& example_query_results) {
  tensorflow::checkpoint::TensorSliceWriter slice_writer(
      output_checkpoint_filename,
      tensorflow::checkpoint::CreateTableTensorSliceBuilder);
  for (auto const& [example_query, example_query_result] :
       example_query_results) {
    for (auto const& [vector_name, vector_tuple] :
         engine::GetOutputVectorSpecs(example_query)) {
      std::string output_name = std::get<0>(vector_tuple);
      ExampleQuerySpec::OutputVectorSpec output_vector_spec =
          std::get<1>(vector_tuple);
      auto it = example_query_result.vector_data().vectors().find(vector_name);
      if (it == example_query_result.vector_data().vectors().end()) {
        return absl::DataLossError(
            "Expected value not found in the example query result");
      }
      const ExampleQueryResult::VectorData::Values values = it->second;
      absl::Status status;
      if (values.has_int32_values()) {
        FCP_RETURN_IF_ERROR(engine::CheckOutputVectorDataType(
            output_vector_spec, DataType::INT32));
        int64_t size = values.int32_values().value_size();
        auto data =
            static_cast<const int32_t*>(values.int32_values().value().data());
        FCP_RETURN_IF_ERROR(WriteSlice(slice_writer, output_name, size, data));
      } else if (values.has_int64_values()) {
        FCP_RETURN_IF_ERROR(engine::CheckOutputVectorDataType(
            output_vector_spec, DataType::INT64));
        int64_t size = values.int64_values().value_size();
        auto data =
            static_cast<const int64_t*>(values.int64_values().value().data());
        FCP_RETURN_IF_ERROR(WriteSlice(slice_writer, output_name, size, data));
      } else if (values.has_string_values()) {
        FCP_RETURN_IF_ERROR(engine::CheckOutputVectorDataType(
            output_vector_spec, DataType::STRING));
        int64_t size = values.string_values().value_size();
        std::vector<tensorflow::tstring> tf_string_vector;
        for (const auto& value : values.string_values().value()) {
          tf_string_vector.emplace_back(value);
        }
        FCP_RETURN_IF_ERROR(WriteSlice(slice_writer, output_name, size,
                                       tf_string_vector.data()));
      } else if (values.has_bool_values()) {
        FCP_RETURN_IF_ERROR(engine::CheckOutputVectorDataType(
            output_vector_spec, DataType::BOOL));
        int64_t size = values.bool_values().value_size();
        auto data =
            static_cast<const bool*>(values.bool_values().value().data());
        FCP_RETURN_IF_ERROR(WriteSlice(slice_writer, output_name, size, data));
      } else if (values.has_float_values()) {
        FCP_RETURN_IF_ERROR(engine::CheckOutputVectorDataType(
            output_vector_spec, DataType::FLOAT));
        int64_t size = values.float_values().value_size();
        auto data =
            static_cast<const float*>(values.float_values().value().data());
        FCP_RETURN_IF_ERROR(WriteSlice(slice_writer, output_name, size, data));
      } else if (values.has_double_values()) {
        FCP_RETURN_IF_ERROR(engine::CheckOutputVectorDataType(
            output_vector_spec, DataType::DOUBLE));
        int64_t size = values.double_values().value_size();
        auto data =
            static_cast<const double*>(values.double_values().value().data());
        FCP_RETURN_IF_ERROR(WriteSlice(slice_writer, output_name, size, data));
      } else if (values.has_bytes_values()) {
        FCP_RETURN_IF_ERROR(engine::CheckOutputVectorDataType(
            output_vector_spec, DataType::BYTES));
        int64_t size = values.bytes_values().value_size();
        std::vector<tensorflow::tstring> tf_string_vector;
        for (const auto& value : values.string_values().value()) {
          tf_string_vector.emplace_back(value);
        }
        FCP_RETURN_IF_ERROR(WriteSlice(slice_writer, output_name, size,
                                       tf_string_vector.data()));
      } else {
        return absl::DataLossError(
            "Unexpected data type in the example query result");
      }
    }
  }
  return slice_writer.Finish();
}

}  // namespace fcp::client
