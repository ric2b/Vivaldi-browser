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
#ifndef FCP_CLIENT_TENSORFLOW_TENSORFLOW_RUNNER_H_
#define FCP_CLIENT_TENSORFLOW_TENSORFLOW_RUNNER_H_

#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/time/time.h"
#include "fcp/client/engine/common.h"
#include "fcp/client/engine/example_iterator_factory.h"
#include "fcp/client/example_iterator_query_recorder.h"
#include "fcp/client/example_query_result.pb.h"
#include "fcp/client/flags.h"
#include "fcp/client/interruptible_runner.h"
#include "fcp/client/log_manager.h"
#include "fcp/client/opstats/opstats_logger.h"
#include "fcp/client/runner_common.h"

namespace fcp::client {

// An interface for running a plan with TensorFlow.
class TensorflowRunner {
 public:
  virtual ~TensorflowRunner() = default;
  // Runs an eligibility eval plan with TensorFlow spec.
  virtual engine::PlanResult RunEligibilityEvalPlanWithTensorflowSpec(
      std::vector<engine::ExampleIteratorFactory*> example_iterator_factories,
      std::function<bool()> should_abort, LogManager* log_manager,
      opstats::OpStatsLogger* opstats_logger, const Flags* flags,
      const google::internal::federated::plan::ClientOnlyPlan& client_plan,
      const std::string& checkpoint_input_filename,
      const fcp::client::InterruptibleRunner::TimingConfig& timing_config,
      absl::Time run_plan_start_time, absl::Time reference_time) = 0;

  // Runs a plan with TensorFlow spec.
  virtual PlanResultAndCheckpointFile RunPlanWithTensorflowSpec(
      std::vector<engine::ExampleIteratorFactory*> example_iterator_factories,
      std::function<bool()> should_abort, LogManager* log_manager,
      opstats::OpStatsLogger* opstats_logger, const Flags* flags,
      ExampleIteratorQueryRecorder* example_iterator_query_recorder,
      const google::internal::federated::plan::ClientOnlyPlan& client_plan,
      const std::string& checkpoint_input_filename,
      const std::string& checkpoint_output_filename,
      const fcp::client::InterruptibleRunner::TimingConfig& timing_config) = 0;

  // Writes example query results to a TF v1 checkpointwith the given filename.
  virtual absl::Status WriteTFV1Checkpoint(
      const std::string& output_checkpoint_filename,
      const std::vector<std::pair<
          google::internal::federated::plan::ExampleQuerySpec::ExampleQuery,
          ExampleQueryResult>>& example_query_results) = 0;
};

}  // namespace fcp::client

#endif  // FCP_CLIENT_TENSORFLOW_TENSORFLOW_RUNNER_H_
