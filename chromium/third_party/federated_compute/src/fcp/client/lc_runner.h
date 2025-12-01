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
#ifndef FCP_CLIENT_LC_RUNNER_H_
#define FCP_CLIENT_LC_RUNNER_H_

#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "fcp/client/event_publisher.h"
#include "fcp/client/flags.h"
#include "fcp/client/log_manager.h"
#include "fcp/client/opstats/opstats_logger.h"
#include "fcp/client/phase_logger.h"
#include "fcp/client/simple_task_environment.h"

namespace fcp {
namespace client {

// Prod entry point for running a local computation. Concurrent calls, with
// the same SimpleTaskEnvironment::GetBaseDir(), are not supported.
// If the training conditions are not met, return CANCELLED status.
// If the plan cannot be parsed, return INVALID_ARGUMENT status.
// If the plan does not contain tensorSpec, return INVALID_ARGUMENT status.
// If the plan does not contain LocalComputeIORouter, return INVALID_ARGUMENT
// status.
// If the plan contains ClientExecutions, return INVALID_ARGUMENT status.
// If the plan expects input tensors other than dataset token, input dir and
// output dir, return INVALID_ARGUMENT status.
// If Tensorflow completes, return OK status.
// If Tensorflow was interrupted, return CANCELLED status.
absl::Status RunLocalComputation(
    SimpleTaskEnvironment* env_deps, EventPublisher* event_publisher,
    LogManager* log_manager, const Flags* flags,
    const std::string& session_name, const std::string& plan_uri,
    const std::string& input_dir_uri, const std::string& output_dir_uri,
    const absl::flat_hash_map<std::string, std::string>& input_resources);

// This is exposed for use in tests that require a mocked OpStatsLogger.
// Otherwise, this is used internally by the other RunLocalComputation
// method once the OpStatsLogger object has been created.
absl::Status RunLocalComputation(
    PhaseLogger& phase_logger, SimpleTaskEnvironment* env_deps,
    LogManager* log_manager,
    ::fcp::client::opstats::OpStatsLogger* opstats_logger, const Flags* flags,
    const std::string& plan_uri, const std::string& input_dir_uri,
    const std::string& output_dir_uri,
    const absl::flat_hash_map<std::string, std::string>& input_resources,
    const SelectorContext& selector_context);

}  // namespace client
}  // namespace fcp

#endif  // FCP_CLIENT_LC_RUNNER_H_
