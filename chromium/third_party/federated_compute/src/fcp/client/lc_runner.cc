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
#include "fcp/client/lc_runner.h"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "fcp/base/monitoring.h"
#include "fcp/base/platform.h"
#include "fcp/client/diag_codes.pb.h"
#include "fcp/client/engine/common.h"
#include "fcp/client/engine/example_iterator_factory.h"
#include "fcp/client/event_publisher.h"
#include "fcp/client/flags.h"
#include "fcp/client/interruptible_runner.h"
#include "fcp/client/log_manager.h"
#include "fcp/client/opstats/opstats_logger.h"
#include "fcp/client/opstats/opstats_logger_impl.h"
#include "fcp/client/phase_logger.h"
#include "fcp/client/simple_task_environment.h"
#include "fcp/client/stats.h"
#include "tensorflow/core/platform/tstring.h"

#ifdef FCP_CLIENT_SUPPORT_TFMOBILE
#include "fcp/client/engine/simple_plan_engine.h"
#endif

#include "fcp/client/engine/tflite_plan_engine.h"
#include "fcp/client/opstats/opstats_example_store.h"
#include "fcp/client/phase_logger_impl.h"
#include "fcp/client/selector_context.pb.h"
#include "fcp/protos/plan.pb.h"
#include "tensorflow/core/framework/tensor.h"
#include "tensorflow/core/framework/tensor.pb.h"
#include "tensorflow/core/framework/tensor_shape.pb.h"
#include "tensorflow/core/protobuf/struct.pb.h"

namespace fcp {
namespace client {

using ::fcp::client::opstats::OpStatsLogger;
using ::google::internal::federated::plan::ClientOnlyPlan;
using ::google::internal::federated::plan::LocalComputeIORouter;

using TfLiteInputs = absl::flat_hash_map<std::string, std::string>;
using TfMobileInputs = std::vector<std::pair<std::string, tensorflow::Tensor>>;

namespace {
#ifdef FCP_CLIENT_SUPPORT_TFMOBILE
absl::StatusOr<std::unique_ptr<TfMobileInputs>>
ConstructInputsForTensorflowSpecPlan(
    const LocalComputeIORouter& local_compute, const std::string& input_dir_uri,
    const std::string& output_dir_uri,
    const absl::flat_hash_map<std::string, std::string>& input_resources) {
  auto inputs = std::make_unique<
      std::vector<std::pair<std::string, tensorflow::Tensor>>>();
  if (local_compute.has_multiple_input_resources()) {
    if (!input_dir_uri.empty()) {
      return absl::InvalidArgumentError(
          "Both input dir and input resources are provided.");
    }
    auto input_resource_tensor_name_map =
        local_compute.multiple_input_resources()
            .input_resource_tensor_name_map();
    for (const auto& resource : input_resources) {
      tensorflow::Tensor resource_tensor(tensorflow::DT_STRING, {});
      resource_tensor.scalar<tensorflow::tstring>()() = resource.second;
      if (!input_resource_tensor_name_map.contains(resource.first)) {
        return absl::InvalidArgumentError(
            absl::StrCat("User provided input resource:", resource.first,
                         " is missing in LocalComputeIORouter."));
      }
      std::string tensor_name = input_resource_tensor_name_map[resource.first];
      inputs->push_back({tensor_name, resource_tensor});
    }
  } else {
    tensorflow::Tensor input_dirpath(tensorflow::DT_STRING, {});
    input_dirpath.scalar<tensorflow::tstring>()() = input_dir_uri;
    inputs->push_back({local_compute.input_dir_tensor_name(), input_dirpath});
  }
  tensorflow::Tensor output_dirpath(tensorflow::DT_STRING, {});
  output_dirpath.scalar<tensorflow::tstring>()() = output_dir_uri;
  inputs->push_back({local_compute.output_dir_tensor_name(), output_dirpath});
  return inputs;
}
#endif

absl::StatusOr<std::unique_ptr<TfLiteInputs>> ConstructInputsForTFLitePlan(
    const LocalComputeIORouter& local_compute, const std::string& input_dir_uri,
    const std::string& output_dir_uri,
    const absl::flat_hash_map<std::string, std::string>& input_resources) {
  auto inputs = std::make_unique<TfLiteInputs>();
  if (local_compute.has_multiple_input_resources()) {
    if (!input_dir_uri.empty()) {
      return absl::InvalidArgumentError(
          "Both input dir and input resources are provided.");
    }
    auto input_resource_tensor_name_map =
        local_compute.multiple_input_resources()
            .input_resource_tensor_name_map();
    for (const auto& resource : input_resources) {
      if (!input_resource_tensor_name_map.contains(resource.first)) {
        // If the user provided more input resources than required in the
        // LocalComputeIORouter, we simply continue without throwing an error.
        // In this way, the user could update their scheduling logic separately
        // from their local computation definitions.
        continue;
      }
      std::string tensor_name = input_resource_tensor_name_map[resource.first];
      (*inputs)[tensor_name] = resource.second;
    }
  } else {
    (*inputs)[local_compute.input_dir_tensor_name()] = input_dir_uri;
  }
  (*inputs)[local_compute.output_dir_tensor_name()] = output_dir_uri;
  return inputs;
}

void LogComputationOutcome(engine::PlanResult plan_result,
                           PhaseLogger& phase_logger,
                           absl::Time run_plan_start_time,
                           absl::Time reference_time) {
  switch (plan_result.outcome) {
    case engine::PlanOutcome::kSuccess:
      phase_logger.LogComputationCompleted(
          plan_result.example_stats, NetworkStats(), run_plan_start_time,
          reference_time, /*min_sep_policy_index=*/std::nullopt);
      break;
    case engine::PlanOutcome::kInterrupted:
      phase_logger.LogComputationInterrupted(
          plan_result.original_status, plan_result.example_stats,
          NetworkStats(), run_plan_start_time, reference_time);
      break;
    case engine::PlanOutcome::kInvalidArgument:
      phase_logger.LogComputationInvalidArgument(
          plan_result.original_status, plan_result.example_stats,
          NetworkStats(), run_plan_start_time);
      break;
    case engine::PlanOutcome::kTensorflowError:
      phase_logger.LogComputationTensorflowError(
          std::move(plan_result.original_status), plan_result.example_stats,
          NetworkStats(), run_plan_start_time, reference_time);
      break;
    case engine::PlanOutcome::kExampleIteratorError:
      phase_logger.LogComputationExampleIteratorError(
          plan_result.original_status, plan_result.example_stats,
          NetworkStats(), run_plan_start_time);
      break;
  }
}

// Creates an ExampleIteratorFactory that routes queries to the
// SimpleTaskEnvironment::CreateExampleIterator() method.
std::unique_ptr<engine::ExampleIteratorFactory>
CreateSimpleTaskEnvironmentIteratorFactory(
    SimpleTaskEnvironment* task_env, const SelectorContext& selector_context) {
  return std::make_unique<engine::FunctionalExampleIteratorFactory>(
      /*can_handle_func=*/
      [](const google::internal::federated::plan::ExampleSelector&) {
        // The SimpleTaskEnvironment-based ExampleIteratorFactory should
        // be the catch-all factory that is able to handle all queries
        // that no other ExampleIteratorFactory is able to handle.
        return true;
      },
      /*create_iterator_func=*/
      [task_env, selector_context](
          const google::internal::federated::plan::ExampleSelector&
              example_selector) {
        return task_env->CreateExampleIterator(example_selector,
                                               selector_context);
      },
      /*should_collect_stats=*/true);
}

absl::Status RunPlanWithTensorflowSpec(
    PhaseLogger& phase_logger,
    std::vector<engine::ExampleIteratorFactory*> example_iterator_factories,
    std::function<bool()> should_abort, LogManager* log_manager,
    OpStatsLogger* opstats_logger, const Flags* flags,
    const ClientOnlyPlan& client_plan, const std::string& input_dir_uri,
    const std::string& output_dir_uri,
    const absl::flat_hash_map<std::string, std::string>& input_resources,
    const fcp::client::InterruptibleRunner::TimingConfig& timing_config,
    const absl::Time run_plan_start_time, const absl::Time reference_time) {
  // Check that this is a TensorflowSpec-based plan for local computation.
  if (!client_plan.phase().has_tensorflow_spec()) {
    absl::Status error_status =
        absl::InvalidArgumentError("Plan without TensorflowSpec");
    phase_logger.LogComputationInvalidArgument(
        error_status, ExampleStats(), NetworkStats(), run_plan_start_time);
    return error_status;
  }
  if (!client_plan.phase().has_local_compute()) {
    absl::Status error_status =
        absl::InvalidArgumentError("Invalid TensorflowSpec-based plan");
    phase_logger.LogComputationInvalidArgument(
        error_status, ExampleStats(), NetworkStats(), run_plan_start_time);
    return error_status;
  }

  // Run plan
  std::vector<std::string> output_names_unused;

  if (!client_plan.tflite_graph().empty()) {
    log_manager->LogDiag(
        ProdDiagCode::BACKGROUND_TRAINING_TFLITE_MODEL_INCLUDED);
  }
  if (flags->use_tflite_training() && !client_plan.tflite_graph().empty()) {
    auto inputs = ConstructInputsForTFLitePlan(
        client_plan.phase().local_compute(), input_dir_uri, output_dir_uri,
        input_resources);
    if (!inputs.ok()) {
      phase_logger.LogComputationInvalidArgument(
          inputs.status(), ExampleStats(), NetworkStats(), run_plan_start_time);
      return inputs.status();
    }
    engine::TfLitePlanEngine plan_engine(
        example_iterator_factories, should_abort, log_manager, opstats_logger,
        flags, /*example_iterator_query_recorder=*/nullptr, &timing_config);
    engine::PlanResult plan_result = plan_engine.RunPlan(
        client_plan.phase().tensorflow_spec(), client_plan.tflite_graph(),
        std::move(*inputs), output_names_unused,
        /*is_eligibility_eval_plan=*/false);
    engine::PlanOutcome outcome = plan_result.outcome;
    LogComputationOutcome(std::move(plan_result), phase_logger,
                          run_plan_start_time, reference_time);
    return ConvertPlanOutcomeToStatus(outcome);
  }

#ifdef FCP_CLIENT_SUPPORT_TFMOBILE
  // Construct input tensors based on the values in the LocalComputeIORouter
  // message.
  auto inputs = ConstructInputsForTensorflowSpecPlan(
      client_plan.phase().local_compute(), input_dir_uri, output_dir_uri,
      input_resources);
  if (!inputs.ok()) {
    phase_logger.LogComputationInvalidArgument(
        inputs.status(), ExampleStats(), NetworkStats(), run_plan_start_time);
    return inputs.status();
  }
  engine::SimplePlanEngine plan_engine(
      example_iterator_factories, should_abort, log_manager, opstats_logger,
      /*example_iterator_query_recorder=*/nullptr, &timing_config);
  engine::PlanResult plan_result = plan_engine.RunPlan(
      client_plan.phase().tensorflow_spec(), client_plan.graph(),
      client_plan.tensorflow_config_proto(), std::move(*inputs),
      output_names_unused, /*is_eligibility_eval_plan=*/false);
  engine::PlanOutcome outcome = plan_result.outcome;
  LogComputationOutcome(std::move(plan_result), phase_logger,
                        run_plan_start_time, reference_time);
  return ConvertPlanOutcomeToStatus(outcome);
#else
  return absl::InternalError("No plan engine enabled");
#endif
}
}  // anonymous namespace

absl::Status RunLocalComputation(
    SimpleTaskEnvironment* env_deps, EventPublisher* event_publisher,
    LogManager* log_manager, const Flags* flags,
    const std::string& session_name, const std::string& plan_uri,
    const std::string& input_dir_uri, const std::string& output_dir_uri,
    const absl::flat_hash_map<std::string, std::string>& input_resources) {
  auto opstats_logger = opstats::CreateOpStatsLogger(
      env_deps->GetBaseDir(), flags, log_manager, session_name,
      /*population_name=*/"");
  SelectorContext selector_context;
  selector_context.mutable_computation_properties()->set_session_name(
      session_name);
  LocalComputation computation = LocalComputation();
  computation.set_input_dir(input_dir_uri);
  computation.set_output_dir(output_dir_uri);
  computation.mutable_input_resource_map()->insert(input_resources.begin(),
                                                   input_resources.end());
  *selector_context.mutable_computation_properties()->mutable_local_compute() =
      computation;
  PhaseLoggerImpl phase_logger(event_publisher, opstats_logger.get(),
                               log_manager, flags);
  return RunLocalComputation(phase_logger, env_deps, log_manager,
                             opstats_logger.get(), flags, plan_uri,
                             input_dir_uri, output_dir_uri, input_resources,
                             selector_context);
}

absl::Status RunLocalComputation(
    PhaseLogger& phase_logger, SimpleTaskEnvironment* env_deps,
    LogManager* log_manager, OpStatsLogger* opstats_logger, const Flags* flags,
    const std::string& plan_uri, const std::string& input_dir_uri,
    const std::string& output_dir_uri,
    const absl::flat_hash_map<std::string, std::string>& input_resources,
    const SelectorContext& selector_context) {
  absl::Time reference_time = absl::Now();
  absl::Duration polling_period =
      absl::Milliseconds(flags->condition_polling_period_millis());
  std::function<bool()> should_abort = [env_deps, polling_period]() {
    return env_deps->ShouldAbort(absl::Now(), polling_period);
  };
  // Check if the device conditions allow running a local computation.
  if (should_abort()) {
    std::string message =
        "Device conditions not satisfied, aborting local computation";
    FCP_LOG(INFO) << message;
    phase_logger.LogTaskNotStarted(message);
    return absl::CancelledError("");
  }
  // Local compute plans can use example iterators from the
  // SimpleTaskEnvironment and those reading the OpStats DB.
  opstats::OpStatsExampleIteratorFactory opstats_example_iterator_factory(
      opstats_logger, log_manager);
  std::unique_ptr<engine::ExampleIteratorFactory> env_example_iterator_factory =
      CreateSimpleTaskEnvironmentIteratorFactory(env_deps, selector_context);
  std::vector<engine::ExampleIteratorFactory*> example_iterator_factories{
      &opstats_example_iterator_factory, env_example_iterator_factory.get()};

  fcp::client::InterruptibleRunner::TimingConfig timing_config = {
      .polling_period = polling_period,
      .graceful_shutdown_period = absl::Milliseconds(
          flags->tf_execution_teardown_grace_period_millis()),
      .extended_shutdown_period = absl::Milliseconds(
          flags->tf_execution_teardown_extended_period_millis()),
  };

  absl::Time run_plan_start_time = absl::Now();
  phase_logger.LogComputationStarted("");

  absl::StatusOr<std::string> plan_str = fcp::ReadFileToString(plan_uri);
  if (!plan_str.ok()) {
    phase_logger.LogComputationIOError(plan_str.status(), ExampleStats(),
                                       NetworkStats(), run_plan_start_time);
    return plan_str.status();
  }

  ClientOnlyPlan plan;
  if (!plan.ParseFromString(*plan_str)) {
    absl::Status error_status =
        absl::InvalidArgumentError("could not parse received plan");
    phase_logger.LogComputationInvalidArgument(
        error_status, ExampleStats(), NetworkStats(), run_plan_start_time);
    return error_status;
  }

  std::vector<std::string> output_names;
  std::vector<tensorflow::Tensor> output_tensors;
  return RunPlanWithTensorflowSpec(
      phase_logger, example_iterator_factories, should_abort, log_manager,
      opstats_logger, flags, plan, input_dir_uri, output_dir_uri,
      input_resources, timing_config, run_plan_start_time, reference_time);
}

}  // namespace client
}  // namespace fcp
