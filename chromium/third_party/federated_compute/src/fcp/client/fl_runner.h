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
#ifndef FCP_CLIENT_FL_RUNNER_H_
#define FCP_CLIENT_FL_RUNNER_H_

#include <optional>
#include <string>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "fcp/base/clock.h"
#include "fcp/client/engine/engine.pb.h"
#include "fcp/client/event_publisher.h"
#include "fcp/client/federated_protocol.h"
#include "fcp/client/federated_select.h"
#include "fcp/client/files.h"
#include "fcp/client/fl_runner.pb.h"
#include "fcp/client/fl_runner_tensorflow_spec_result.pb.h"
#include "fcp/client/flags.h"
#include "fcp/client/interruptible_runner.h"
#include "fcp/client/log_manager.h"
#include "fcp/client/opstats/opstats_logger.h"
#include "fcp/client/phase_logger.h"
#include "fcp/client/simple_task_environment.h"
#include "fcp/protos/plan.pb.h"
#include "fcp/protos/population_eligibility_spec.pb.h"

namespace fcp {
namespace client {

inline constexpr absl::string_view kTensorflowCheckpointAggregand =
    "tensorflow_checkpoint";

inline constexpr absl::string_view kFederatedComputeCheckpoint =
    "federated_compute_checkpoint";

// Prod entry point for running a federated computation. Concurrent calls, with
// the same SimpleTaskEnvironment::GetBaseDir(), are not supported.
//
// This is a long running blocking call that - for a successful run -
// encompasses connecting to a server, downloading and running a computation,
// uploading results, and storing logs about the run in an operational stats DB.
// During that call, the function will call back (from both the calling and from
// newly created threads) into the dependencies injected here for to query for
// examples, check whether it should abort, publish events / logs for telemetry,
// create files, and query feature flags.
//
// Arguments:
// - federated_service_uri, api_key: used to connect to the Federated server.
// - test_cert_path: a file path to a CA certificate to be used in tests. Should
//     be empty for production use; when used in tests, the URI must use the
//     https+test:// scheme.
// - session_name: A client-side identifier of the type of work this computation
//     performs; used to annotate log entries in the operational stats DB.
// - population_name: a string provided to the Federated server to identify
//     what population this device is checking in for.
// - client_version: A platform-specific identifier that is used by the server
//     to serve versioned computations - that is, versions of a computation that
//     have been tested and found to be compatible with this device's version -
//     or reject the device.
// - client_attestation_measurement: An opaque string from a "measurement" that
//     can be used by the server to attest the device integrity.
//
// Returns:
// On success, the returned FLRunnerResult contains information on when the
// function should be called again for this session.
absl::StatusOr<FLRunnerResult> RunFederatedComputation(
    SimpleTaskEnvironment* env_deps, EventPublisher* event_publisher,
    Files* files, LogManager* log_manager, const Flags* flags,
    const std::string& federated_service_uri, const std::string& api_key,
    const std::string& test_cert_path, const std::string& session_name,
    const std::string& population_name, const std::string& retry_token,
    const std::string& client_version,
    const std::string& client_attestation_measurement);

// This is exposed for use in tests that require a mocked FederatedProtocol and
// OpStatsLogger. Otherwise, this is used internally by the other
// RunFederatedComputation method once the FederatedProtocol and OpStatsLogger
// objects have been created.
absl::StatusOr<FLRunnerResult> RunFederatedComputation(
    SimpleTaskEnvironment* env_deps, PhaseLogger& phase_logger,
    EventPublisher* event_publisher, Files* files, LogManager* log_manager,
    ::fcp::client::opstats::OpStatsLogger* opstats_logger, const Flags* flags,
    FederatedProtocol* federated_protocol,
    FederatedSelectManager* fedselect_manager,
    const fcp::client::InterruptibleRunner::TimingConfig& timing_config,
    absl::Time reference_time, const std::string& session_name,
    const std::string& population_name, Clock& clock);

// This is exposed for use in compatibility tests only. Prod code should call
// RunFederatedComputation.
FLRunnerTensorflowSpecResult RunPlanWithTensorflowSpecForTesting(
    SimpleTaskEnvironment* env_deps, EventPublisher* event_publisher,
    Files* files, LogManager* log_manager, const Flags* flags,
    const google::internal::federated::plan::ClientOnlyPlan& client_plan,
    const std::string& checkpoint_input_filename,
    const fcp::client::InterruptibleRunner::TimingConfig& timing_config,
    absl::Time run_plan_start_time, absl::Time reference_time,
    std::optional<
        ::google::internal::federated::plan::PopulationEligibilitySpec>
        population_eligibility_spec);

}  // namespace client
}  // namespace fcp

#endif  // FCP_CLIENT_FL_RUNNER_H_
