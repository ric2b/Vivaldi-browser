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

#include <fstream>
#include <optional>
#include <string>
#include <utility>


#include "absl/flags/flag.h"
#include "absl/flags/parse.h"  // IWYU pragma: keep
#include "absl/flags/usage.h"  // IWYU pragma: keep
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "fcp/base/monitoring.h"
#include "fcp/client/client_runner.h"
#include "fcp/client/client_runner_example_data.pb.h"
#include "fcp/client/fake_event_publisher.h"
#include "fcp/client/fl_runner.h"

ABSL_FLAG(std::string, server, "",
          "Federated Server URI (supports https:// URIs");
ABSL_FLAG(std::string, api_key, "", "API Key");
ABSL_FLAG(std::string, test_cert, "", "Path to test CA certificate PEM file");
ABSL_FLAG(std::string, session, "", "Session name");
ABSL_FLAG(std::string, population, "", "Population name");
ABSL_FLAG(std::string, retry_token, "", "Retry token");
ABSL_FLAG(std::string, client_version, "", "Client version");
ABSL_FLAG(std::string, client_attestation_measurement, "",
          "Client attestation measurement");
ABSL_FLAG(std::string, example_data_path, "",
          "Path to a serialized ClientRunnerExampleData proto with client "
          "example data. Falls back to --num_empty_examples if unset.");
ABSL_FLAG(int, num_empty_examples, 0,
          "Number of (empty) examples each created iterator serves. Ignored if "
          "--example_store_path is set.");
ABSL_FLAG(int, num_rounds, 1, "Number of rounds to train");
ABSL_FLAG(int, sleep_after_round_secs, 3,
          "Number of seconds to sleep after each round.");

static constexpr char kUsageString[] =
    "Stand-alone Federated Client Executable.\n\n"
    "Connects to the specified server, tries to retrieve a plan, run the\n"
    "plan (feeding the specified number of empty examples), and report the\n"
    "results of the computation back to the server.";

static absl::StatusOr<fcp::client::ClientRunnerExampleData> LoadExampleData(
    const std::string& examples_path) {
  std::ifstream examples_file(examples_path);
  fcp::client::ClientRunnerExampleData data;
  if (!data.ParseFromIstream(&examples_file) || !examples_file.eof()) {
    return absl::InvalidArgumentError(
        "Failed to parse ClientRunnerExampleData");
  }
  return data;
}

int main(int argc, char** argv) {
  absl::SetProgramUsageMessage(kUsageString);
  absl::ParseCommandLine(argc, argv);

  int num_rounds = absl::GetFlag(FLAGS_num_rounds);
  std::string server = absl::GetFlag(FLAGS_server);
  std::string session = absl::GetFlag(FLAGS_session);
  std::string population = absl::GetFlag(FLAGS_population);
  std::string client_version = absl::GetFlag(FLAGS_client_version);
  std::string test_cert = absl::GetFlag(FLAGS_test_cert);
  FCP_LOG(INFO) << "Running for " << num_rounds << " rounds:";
  FCP_LOG(INFO) << " - server:         " << server;
  FCP_LOG(INFO) << " - session:        " << session;
  FCP_LOG(INFO) << " - population:     " << population;
  FCP_LOG(INFO) << " - client_version: " << client_version;

  std::optional<fcp::client::ClientRunnerExampleData> example_data;
  if (std::string path = absl::GetFlag(FLAGS_example_data_path);
      !path.empty()) {
    auto statusor = LoadExampleData(path);
    if (!statusor.ok()) {
      FCP_LOG(ERROR) << "Failed to load example data: " << statusor.status();
      return 1;
    }
    example_data = *std::move(statusor);
  }

  bool success = false;
  for (auto i = 0; i < num_rounds || num_rounds < 0; ++i) {
    fcp::client::FederatedTaskEnvDepsImpl federated_task_env_deps_impl =
        example_data
            ? fcp::client::FederatedTaskEnvDepsImpl(*example_data, test_cert)
            : fcp::client::FederatedTaskEnvDepsImpl(
                  absl::GetFlag(FLAGS_num_empty_examples), test_cert);
    fcp::client::FakeEventPublisher event_publisher(/*quiet=*/false);
    fcp::client::FilesImpl files_impl;
    fcp::client::LogManagerImpl log_manager_impl;
    fcp::client::FlagsImpl flags;

    auto fl_runner_result = RunFederatedComputation(
        &federated_task_env_deps_impl, &event_publisher, &files_impl,
        &log_manager_impl, &flags, server, absl::GetFlag(FLAGS_api_key),
        test_cert, session, population, absl::GetFlag(FLAGS_retry_token),
        client_version, absl::GetFlag(FLAGS_client_attestation_measurement));
    if (fl_runner_result.ok()) {
      FCP_LOG(INFO) << "Run finished successfully; result: "
                    << fl_runner_result.value().DebugString();
      success = true;
    } else {
      FCP_LOG(ERROR) << "Error during run: " << fl_runner_result.status();
    }
    int sleep_secs = absl::GetFlag(FLAGS_sleep_after_round_secs);
    FCP_LOG(INFO) << "Sleeping for " << sleep_secs << " secs";
    absl::SleepFor(absl::Seconds(sleep_secs));
  }
  return success ? 0 : 1;
}
