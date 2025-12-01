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
#ifndef FCP_CLIENT_ENGINE_TFLITE_PLAN_ENGINE_H_
#define FCP_CLIENT_ENGINE_TFLITE_PLAN_ENGINE_H_

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "fcp/client/engine/common.h"
#include "fcp/client/engine/example_iterator_factory.h"
#include "fcp/client/example_iterator_query_recorder.h"
#include "fcp/client/flags.h"
#include "fcp/client/interruptible_runner.h"
#include "fcp/client/log_manager.h"
#include "fcp/client/opstats/opstats_logger.h"

namespace fcp {
namespace client {
namespace engine {

// A class used to "run" (interpret) a TensorflowSpec-based plan with TfLite.
// Each instance should generally only be used once to run a plan.
class TfLitePlanEngine {
 public:
  // For each example query issued by the plan at runtime, the given
  // `example_iterator_factories` parameter will be iterated and the first
  // iterator factory that can handle the given query will be used to create the
  // example iterator for that query.
  TfLitePlanEngine(
      std::vector<ExampleIteratorFactory*> example_iterator_factories,
      std::function<bool()> should_abort, LogManager* log_manager,
      ::fcp::client::opstats::OpStatsLogger* opstats_logger, const Flags* flags,
      ExampleIteratorQueryRecorder* example_iterator_query_recorder,
      const InterruptibleRunner::TimingConfig* timing_config)
      : example_iterator_factories_(example_iterator_factories),
        should_abort_(should_abort),
        log_manager_(log_manager),
        opstats_logger_(opstats_logger),
        flags_(*flags),
        example_iterator_query_recorder_(example_iterator_query_recorder),
        timing_config_(timing_config) {}

  // Runs the plan, and takes care of logging TfLite errors and external
  // interruptions via event_publisher. If the TfLite call fails because it got
  // aborted externally, returns CANCELLED. If the TfLite call fails because of
  // other reasons, publishes an event, then returns INVALID_ARGUMENT. If the
  // TfLite call is successful, returns OK, and the output tensors.
  PlanResult RunPlan(
      const google::internal::federated::plan::TensorflowSpec& tensorflow_spec,
      const std::string& model,
      std::unique_ptr<absl::flat_hash_map<std::string, std::string>> inputs,
      const std::vector<std::string>& output_names,
      bool is_eligibility_eval_plan);

 private:
  std::vector<ExampleIteratorFactory*> example_iterator_factories_;
  std::function<bool()> should_abort_;
  LogManager* log_manager_;
  ::fcp::client::opstats::OpStatsLogger* opstats_logger_;
  const Flags& flags_;
  ExampleIteratorQueryRecorder* example_iterator_query_recorder_;
  const InterruptibleRunner::TimingConfig* timing_config_;
};

}  // namespace engine
}  // namespace client
}  // namespace fcp

#endif  // FCP_CLIENT_ENGINE_TFLITE_PLAN_ENGINE_H_
