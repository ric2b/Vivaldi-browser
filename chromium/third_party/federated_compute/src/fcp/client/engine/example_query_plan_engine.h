/*
 * Copyright 2023 Google LLC
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
#ifndef FCP_CLIENT_ENGINE_EXAMPLE_QUERY_PLAN_ENGINE_H_
#define FCP_CLIENT_ENGINE_EXAMPLE_QUERY_PLAN_ENGINE_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/strings/string_view.h"
#include "fcp/client/engine/common.h"
#include "fcp/client/engine/example_iterator_factory.h"
#include "fcp/client/example_iterator_query_recorder.h"
#include "fcp/client/opstats/opstats_logger.h"
#include "fcp/client/tensorflow/tensorflow_runner.h"
#include "tensorflow_federated/cc/core/impl/aggregation/protocol/federated_compute_checkpoint_builder.h"

namespace fcp {
namespace client {
namespace engine {

// A class used to "run" (interpret) an ExampleQuerySpec-based plan. Each
// instance should generally only be used once to run a plan.
class ExampleQueryPlanEngine {
 public:
  ExampleQueryPlanEngine(
      std::vector<ExampleIteratorFactory*> example_iterator_factories,
      ::fcp::client::opstats::OpStatsLogger* opstats_logger,
      ExampleIteratorQueryRecorder* example_iterator_query_recorder,
      // The tensorflow_runner_factory is only used for legacy ExampleQuerySpec
      // that use the TF v1 checkpoint format. The factory may be empty if no
      // such specs will be used (the engine will return an UNIMPLEMENTED error
      // if such plans are encountered in that case).
      const absl::AnyInvocable<std::unique_ptr<TensorflowRunner>() const>&
          tensorflow_runner_factory);

  // Runs a plan and writes an output into a checkpoint at the given path.
  ::fcp::client::engine::PlanResult RunPlan(
      const google::internal::federated::plan::ExampleQuerySpec&
          example_query_spec,
      const std::string& output_checkpoint_filename,
      bool use_client_report_wire_format, bool enable_event_time_data_upload,
      std::optional<absl::string_view> source_id, bool uses_confidential_agg,
      bool enable_privacy_id_generation);

 private:
  std::vector<ExampleIteratorFactory*> example_iterator_factories_;
  ::fcp::client::opstats::OpStatsLogger* opstats_logger_;
  tensorflow_federated::aggregation::FederatedComputeCheckpointBuilderFactory
      federated_compute_checkpoint_builder_factory_;
  ExampleIteratorQueryRecorder* example_iterator_query_recorder_;
  const absl::AnyInvocable<std::unique_ptr<TensorflowRunner>() const>&
      tensorflow_runner_factory_;
};
}  // namespace engine
}  // namespace client
}  // namespace fcp

#endif  // FCP_CLIENT_ENGINE_EXAMPLE_QUERY_PLAN_ENGINE_H_
