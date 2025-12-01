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

#ifndef FCP_CLIENT_ELIGIBILITY_DECIDER_H_
#define FCP_CLIENT_ELIGIBILITY_DECIDER_H_

#include <vector>

#include "absl/status/statusor.h"
#include "fcp/base/clock.h"
#include "fcp/client/engine/example_iterator_factory.h"
#include "fcp/client/flags.h"
#include "fcp/client/log_manager.h"
#include "fcp/client/phase_logger.h"
#include "fcp/protos/federated_api.pb.h"
#include "fcp/protos/opstats.pb.h"
#include "fcp/protos/population_eligibility_spec.pb.h"

namespace fcp::client {

using ::google::internal::federated::plan::PopulationEligibilitySpec;
using ::google::internal::federatedml::v2::TaskEligibilityInfo;

// Pure virtual interface for helper class so we can mock it in tests.
class EetPlanRunner {
 public:
  virtual ~EetPlanRunner() = default;
  virtual absl::StatusOr<TaskEligibilityInfo> RunPlan(
      std::vector<engine::ExampleIteratorFactory*>
          example_iterator_factories) = 0;
};

// Computes the eligibility of the client for the given tasks in the population
// eligibility spec.
//
// Returns:
// - On success, returns a filled out TaskEligibilityInfo for the tasks in the
// PopulationEligibilitySpec.
// - If the eligibility could not be decided, i.e. due to an insufficient
// implementation, returns an OK status wrapping an empty TaskEligibilityInfo.
// It is up to the caller to decide what to do.
// - On failure, returns an error status for unrecoverable errors (IO, etc).
absl::StatusOr<TaskEligibilityInfo> ComputeEligibility(
    const PopulationEligibilitySpec& eligibility_spec, LogManager& log_manager,
    PhaseLogger& phase_logger, const opstats::OpStatsSequence& opstats_sequence,
    Clock& clock,
    std::vector<engine::ExampleIteratorFactory*> example_iterator_factories,
    EetPlanRunner& eet_plan_runner, const Flags* flags);

}  // namespace fcp::client

#endif  // FCP_CLIENT_ELIGIBILITY_DECIDER_H_
