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
#ifndef FCP_CLIENT_TEST_HELPERS_H_
#define FCP_CLIENT_TEST_HELPERS_H_

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "google/protobuf/duration.pb.h"
#include "gmock/gmock.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "fcp/client/attestation/attestation_verifier.h"
#include "fcp/client/diag_codes.pb.h"
#include "fcp/client/engine/common.h"
#include "fcp/client/engine/engine.pb.h"
#include "fcp/client/engine/example_iterator_factory.h"
#include "fcp/client/event_publisher.h"
#include "fcp/client/example_iterator_query_recorder.h"
#include "fcp/client/federated_protocol.h"
#include "fcp/client/federated_select.h"
#include "fcp/client/flags.h"
#include "fcp/client/histogram_counters.pb.h"
#include "fcp/client/http/http_client.h"
#include "fcp/client/interruptible_runner.h"
#include "fcp/client/log_manager.h"
#include "fcp/client/opstats/opstats_db.h"
#include "fcp/client/opstats/opstats_logger.h"
#include "fcp/client/phase_logger.h"
#include "fcp/client/runner_common.h"
#include "fcp/client/secagg_event_publisher.h"
#include "fcp/client/secagg_runner.h"
#include "fcp/client/selector_context.pb.h"
#include "fcp/client/simple_task_environment.h"
#include "fcp/client/stats.h"
#include "fcp/client/task_result_info.pb.h"
#include "fcp/client/tensorflow/tensorflow_runner.h"
#include "fcp/protos/confidentialcompute/payload_metadata.pb.h"
#include "fcp/protos/federated_api.pb.h"
#include "fcp/protos/federatedcompute/confidential_aggregations.pb.h"
#include "fcp/protos/opstats.pb.h"
#include "fcp/protos/plan.pb.h"
#include "fcp/protos/population_eligibility_spec.pb.h"
#include "fcp/secagg/shared/secagg_messages.pb.h"
#include "google/protobuf/repeated_ptr_field.h"
#include "tensorflow/core/example/example.pb.h"
#include "tensorflow/core/example/feature.pb.h"

namespace fcp {
namespace client {

// Loads the contents of a file into a string.
bool LoadFileAsString(std::string path, std::string* msg);

class MockSecAggEventPublisher : public SecAggEventPublisher {
 public:
  MOCK_METHOD(void, PublishStateTransition,
              (::fcp::secagg::ClientState state, size_t last_sent_message_size,
               size_t last_received_message_size),
              (override));
  MOCK_METHOD(void, PublishError, (), (override));
  MOCK_METHOD(void, PublishAbort,
              (bool client_initiated, const std::string& error_message),
              (override));
  MOCK_METHOD(void, set_execution_session_id, (int64_t execution_session_id),
              (override));
};

class MockEventPublisher : public EventPublisher {
 public:
  MOCK_METHOD(void, PublishEligibilityEvalCheckin, (), (override));
  MOCK_METHOD(void, PublishEligibilityEvalPlanUriReceived,
              (const NetworkStats& network_stats,
               absl::Duration phase_duration),
              (override));
  MOCK_METHOD(void, PublishEligibilityEvalPlanReceived,
              (const NetworkStats& network_stats,
               absl::Duration phase_duration),
              (override));
  MOCK_METHOD(void, PublishEligibilityEvalNotConfigured,
              (const NetworkStats& network_stats,
               absl::Duration phase_duration),
              (override));
  MOCK_METHOD(void, PublishEligibilityEvalRejected,
              (const NetworkStats& network_stats,
               absl::Duration phase_duration),
              (override));
  MOCK_METHOD(void, PublishCheckin, (), (override));
  MOCK_METHOD(void, PublishCheckinFinished,
              (const NetworkStats& network_stats,
               absl::Duration phase_duration),
              (override));
  MOCK_METHOD(void, PublishRejected, (), (override));
  MOCK_METHOD(void, PublishTensorFlowError,
              (int example_count, absl::string_view error_message), (override));
  MOCK_METHOD(void, PublishIoError, (absl::string_view error_message),
              (override));
  MOCK_METHOD(void, PublishExampleSelectorError,
              (int example_count, absl::string_view error_message), (override));
  MOCK_METHOD(void, PublishInterruption,
              (const ExampleStats& example_stats, absl::Time start_time),
              (override));
  MOCK_METHOD(void, SetModelIdentifier, (const std::string& model_identifier),
              (override));
  MOCK_METHOD(void, PublishTaskNotStarted, (absl::string_view error_message),
              (override));
  MOCK_METHOD(void, PublishNonfatalInitializationError,
              (absl::string_view error_message), (override));
  MOCK_METHOD(void, PublishFatalInitializationError,
              (absl::string_view error_message), (override));
  MOCK_METHOD(void, PublishEligibilityEvalCheckinIoError,
              (absl::string_view error_message,
               const NetworkStats& network_stats,
               absl::Duration phase_duration),
              (override));
  MOCK_METHOD(void, PublishEligibilityEvalCheckinClientInterrupted,
              (absl::string_view error_message,
               const NetworkStats& network_stats,
               absl::Duration phase_duration),
              (override));
  MOCK_METHOD(void, PublishEligibilityEvalCheckinServerAborted,
              (absl::string_view error_message,
               const NetworkStats& network_stats,
               absl::Duration phase_duration),
              (override));
  MOCK_METHOD(void, PublishEligibilityEvalCheckinErrorInvalidPayload,
              (absl::string_view error_message,
               const NetworkStats& network_stats,
               absl::Duration phase_duration),
              (override));
  MOCK_METHOD(void, PublishEligibilityEvalComputationStarted, (), (override));
  MOCK_METHOD(void, PublishEligibilityEvalComputationInvalidArgument,
              (absl::string_view error_message,
               const ExampleStats& example_stats,
               absl::Duration phase_duration),
              (override));
  MOCK_METHOD(void, PublishEligibilityEvalComputationExampleIteratorError,
              (absl::string_view error_message,
               const ExampleStats& example_stats,
               absl::Duration phase_duration),
              (override));
  MOCK_METHOD(void, PublishEligibilityEvalComputationTensorflowError,
              (absl::string_view error_message,
               const ExampleStats& example_stats,
               absl::Duration phase_duration),
              (override));
  MOCK_METHOD(void, PublishEligibilityEvalComputationIOError,
              (absl::string_view error_message,
               const ExampleStats& example_stats,
               absl::Duration phase_duration),
              (override));
  MOCK_METHOD(void, PublishEligibilityEvalComputationInterrupted,
              (absl::string_view error_message,
               const ExampleStats& example_stats,
               absl::Duration phase_duration),
              (override));
  MOCK_METHOD(void, PublishEligibilityEvalComputationErrorNonfatal,
              (absl::string_view error_message), (override));
  MOCK_METHOD(void, PublishEligibilityEvalComputationCompleted,
              (const ExampleStats& example_stats,
               absl::Duration phase_duration),
              (override));
  MOCK_METHOD(void, PublishMultipleTaskAssignmentsStarted, (), (override));
  MOCK_METHOD(void, PublishMultipleTaskAssignmentsIOError,
              (absl::string_view error_message,
               const NetworkStats& network_stats,
               absl::Duration phase_duration),
              (override));
  MOCK_METHOD(void, PublishMultipleTaskAssignmentsPayloadIOError,
              (absl::string_view error_message), (override));
  MOCK_METHOD(void, PublishMultipleTaskAssignmentsInvalidPayload,
              (absl::string_view error_message), (override));
  MOCK_METHOD(void, PublishMultipleTaskAssignmentsClientInterrupted,
              (absl::string_view error_message,
               const NetworkStats& network_stats,
               absl::Duration phase_duration),
              (override));
  MOCK_METHOD(void, PublishMultipleTaskAssignmentsServerAborted,
              (absl::string_view error_message,
               const NetworkStats& network_stats,
               absl::Duration phase_duration),
              (override));
  MOCK_METHOD(void, PublishMultipleTaskAssignmentsTurnedAway,
              (const NetworkStats& network_stats,
               absl::Duration phase_duration),
              (override));
  MOCK_METHOD(void, PublishMultipleTaskAssignmentsPlanUriReceived,
              (const NetworkStats& network_stats,
               absl::Duration phase_duration),
              (override));
  MOCK_METHOD(void, PublishMultipleTaskAssignmentsPlanUriPartialReceived,
              (const NetworkStats& network_stats,
               absl::Duration phase_duration),
              (override));
  MOCK_METHOD(void, PublishMultipleTaskAssignmentsPartialCompleted,
              (const NetworkStats& network_stats,
               absl::Duration phase_duration),
              (override));
  MOCK_METHOD(void, PublishMultipleTaskAssignmentsCompleted,
              (const NetworkStats& network_stats,
               absl::Duration phase_duration),
              (override));
  MOCK_METHOD(void, PublishCheckinIoError,
              (absl::string_view error_message,
               const NetworkStats& network_stats,
               absl::Duration phase_duration),
              (override));
  MOCK_METHOD(void, PublishCheckinClientInterrupted,
              (absl::string_view error_message,
               const NetworkStats& network_stats,
               absl::Duration phase_duration),
              (override));
  MOCK_METHOD(void, PublishCheckinServerAborted,
              (absl::string_view error_message,
               const NetworkStats& network_stats,
               absl::Duration phase_duration),
              (override));
  MOCK_METHOD(void, PublishCheckinInvalidPayload,
              (absl::string_view error_message,
               const NetworkStats& network_stats,
               absl::Duration phase_duration),
              (override));
  MOCK_METHOD(void, PublishRejected,
              (const NetworkStats& network_stats,
               absl::Duration phase_duration),
              (override));
  MOCK_METHOD(void, PublishCheckinPlanUriReceived,
              (const NetworkStats& network_stats,
               absl::Duration phase_duration),
              (override));
  MOCK_METHOD(void, PublishCheckinFinishedV2,
              (const NetworkStats& network_stats,
               absl::Duration phase_duration),
              (override));
  MOCK_METHOD(void, PublishComputationStarted, (), (override));
  MOCK_METHOD(void, PublishComputationInvalidArgument,
              (absl::string_view error_message,
               const ExampleStats& example_stats,
               const NetworkStats& network_stats,
               absl::Duration phase_duration),
              (override));
  MOCK_METHOD(void, PublishComputationIOError,
              (absl::string_view error_message,
               const ExampleStats& example_stats,
               const NetworkStats& network_stats,
               absl::Duration phase_duration),
              (override));
  MOCK_METHOD(void, PublishComputationExampleIteratorError,
              (absl::string_view error_message,
               const ExampleStats& example_stats,
               const NetworkStats& network_stats,
               absl::Duration phase_duration),
              (override));
  MOCK_METHOD(void, PublishComputationTensorflowError,
              (absl::string_view error_message,
               const ExampleStats& example_stats,
               const NetworkStats& network_stats,
               absl::Duration phase_duration),
              (override));
  MOCK_METHOD(void, PublishComputationInterrupted,
              (absl::string_view error_message,
               const ExampleStats& example_stats,
               const NetworkStats& network_stats,
               absl::Duration phase_duration),
              (override));
  MOCK_METHOD(void, PublishComputationCompleted,
              (const ExampleStats& example_stats,
               const NetworkStats& network_stats,
               absl::Duration phase_duration),
              (override));
  MOCK_METHOD(void, PublishResultUploadStarted, (), (override));
  MOCK_METHOD(void, PublishResultUploadIOError,
              (absl::string_view error_message,
               const NetworkStats& network_stats,
               absl::Duration phase_duration),
              (override));
  MOCK_METHOD(void, PublishResultUploadClientInterrupted,
              (absl::string_view error_message,
               const NetworkStats& network_stats,
               absl::Duration phase_duration),
              (override));
  MOCK_METHOD(void, PublishResultUploadServerAborted,
              (absl::string_view error_message,
               const NetworkStats& network_stats,
               absl::Duration phase_duration),
              (override));
  MOCK_METHOD(void, PublishResultUploadCompleted,
              (const NetworkStats& network_stats,
               absl::Duration phase_duration),
              (override));
  MOCK_METHOD(void, PublishFailureUploadStarted, (), (override));
  MOCK_METHOD(void, PublishFailureUploadIOError,
              (absl::string_view error_message,
               const NetworkStats& network_stats,
               absl::Duration phase_duration),
              (override));
  MOCK_METHOD(void, PublishFailureUploadClientInterrupted,
              (absl::string_view error_message,
               const NetworkStats& network_stats,
               absl::Duration phase_duration),
              (override));
  MOCK_METHOD(void, PublishFailureUploadServerAborted,
              (absl::string_view error_message,
               const NetworkStats& network_stats,
               absl::Duration phase_duration),
              (override));
  MOCK_METHOD(void, PublishFailureUploadCompleted,
              (const NetworkStats& network_stats,
               absl::Duration phase_duration),
              (override));

  SecAggEventPublisher* secagg_event_publisher() override {
    return &secagg_event_publisher_;
  }

 private:
  ::testing::NiceMock<MockSecAggEventPublisher> secagg_event_publisher_;
};

// A mock FederatedProtocol implementation, which keeps track of the stages in
// the protocol and returns a different set of network stats and RetryWindow for
// each stage, making it easier to write accurate assertions in unit tests.
class MockFederatedProtocol : public FederatedProtocol {
 public:
  constexpr static NetworkStats kEligibilityCheckinPlanUriReceivedNetworkStats =
      {.bytes_downloaded = 20,
       .bytes_uploaded = 20,
       .network_duration = absl::Milliseconds(25)};
  constexpr static NetworkStats
      kEligibilityCheckinArtifactRetrievalNetworkStats = {
          .bytes_downloaded = 280,
          .bytes_uploaded = 380,
          .network_duration = absl::Milliseconds(25)};
  constexpr static NetworkStats kReportEligibilityEvalErrorNetworkStats = {
      .bytes_downloaded = 120,
      .bytes_uploaded = 120,
      .network_duration = absl::Milliseconds(50)};
  constexpr static NetworkStats
      kMultipleTaskAssignmentsPlanUriReceivedNetworkStats = {
          .bytes_downloaded = 500,
          .bytes_uploaded = 1000,
          .network_duration = absl::Milliseconds(100)};
  constexpr static NetworkStats
      kMultipleTaskAssignmentsArtifactRetrievalNetworkStats = {
          .bytes_downloaded = 1500,
          .bytes_uploaded = 2000,
          .network_duration = absl::Milliseconds(200)};
  constexpr static NetworkStats kCheckinPlanUriReceivedNetworkStats = {
      .bytes_downloaded = 400,
      .bytes_uploaded = 600,
      .network_duration = absl::Milliseconds(70)};
  constexpr static NetworkStats kCheckinArtifactRetrievalNetworkStats = {
      .bytes_downloaded = 800,
      .bytes_uploaded = 1000,
      .network_duration = absl::Milliseconds(100)};
  constexpr static NetworkStats kReportCompletedNetworkStats = {
      .bytes_downloaded = 10000,
      .bytes_uploaded = 20000,
      .network_duration = absl::Milliseconds(350)};
  constexpr static NetworkStats kReportNotCompletedNetworkStats = {
      .bytes_downloaded = 500,
      .bytes_uploaded = 1200,
      .network_duration = absl::Milliseconds(150)};

  static google::internal::federatedml::v2::RetryWindow
  GetInitialRetryWindow() {
    google::internal::federatedml::v2::RetryWindow retry_window;
    retry_window.mutable_delay_min()->set_seconds(0L);
    retry_window.mutable_delay_max()->set_seconds(1L);
    *retry_window.mutable_retry_token() = "INITIAL";
    return retry_window;
  }

  static google::internal::federatedml::v2::RetryWindow
  GetPostEligibilityCheckinRetryWindow() {
    google::internal::federatedml::v2::RetryWindow retry_window;
    retry_window.mutable_delay_min()->set_seconds(100L);
    retry_window.mutable_delay_max()->set_seconds(101L);
    *retry_window.mutable_retry_token() = "POST_ELIGIBILITY";
    return retry_window;
  }

  static google::internal::federatedml::v2::RetryWindow
  GetPostCheckinRetryWindow() {
    google::internal::federatedml::v2::RetryWindow retry_window;
    retry_window.mutable_delay_min()->set_seconds(200L);
    retry_window.mutable_delay_max()->set_seconds(201L);
    *retry_window.mutable_retry_token() = "POST_CHECKIN";
    return retry_window;
  }

  static google::internal::federatedml::v2::RetryWindow
  GetPostReportCompletedRetryWindow() {
    google::internal::federatedml::v2::RetryWindow retry_window;
    retry_window.mutable_delay_min()->set_seconds(300L);
    retry_window.mutable_delay_max()->set_seconds(301L);
    *retry_window.mutable_retry_token() = "POST_REPORT_COMPLETED";
    return retry_window;
  }

  static google::internal::federatedml::v2::RetryWindow
  GetPostReportNotCompletedRetryWindow() {
    google::internal::federatedml::v2::RetryWindow retry_window;
    retry_window.mutable_delay_min()->set_seconds(400L);
    retry_window.mutable_delay_max()->set_seconds(401L);
    *retry_window.mutable_retry_token() = "POST_REPORT_NOT_COMPLETED";
    return retry_window;
  }

  explicit MockFederatedProtocol() = default;

  // We override the real FederatedProtocol methods so that we can intercept the
  // progression of protocol stages, and expose dedicate gMock-overridable
  // methods for use in tests.
  absl::StatusOr<EligibilityEvalCheckinResult> EligibilityEvalCheckin(
      std::function<void(const EligibilityEvalTask&)>
          payload_uris_received_callback) final {
    absl::StatusOr<EligibilityEvalCheckinResult> result =
        MockEligibilityEvalCheckin();
    network_stats_ += kEligibilityCheckinPlanUriReceivedNetworkStats;
    if (result.ok() &&
        std::holds_alternative<FederatedProtocol::EligibilityEvalTask>(
            *result)) {
      payload_uris_received_callback(
          std::get<FederatedProtocol::EligibilityEvalTask>(*result));
      network_stats_ += kEligibilityCheckinArtifactRetrievalNetworkStats;
    }
    retry_window_ = GetPostEligibilityCheckinRetryWindow();
    return result;
  };
  MOCK_METHOD(absl::StatusOr<EligibilityEvalCheckinResult>,
              MockEligibilityEvalCheckin, ());

  void ReportEligibilityEvalError(absl::Status error_status) final {
    network_stats_ += kReportEligibilityEvalErrorNetworkStats;
    retry_window_ = GetPostEligibilityCheckinRetryWindow();
    MockReportEligibilityEvalError(error_status);
  }
  MOCK_METHOD(void, MockReportEligibilityEvalError,
              (absl::Status error_status));

  absl::StatusOr<CheckinResult> Checkin(
      const std::optional<
          ::google::internal::federatedml::v2::TaskEligibilityInfo>&
          task_eligibility_info,
      std::function<void(const FederatedProtocol::TaskAssignment&)>
          payload_uris_received_callback,
      const std::optional<std::string>& attestation_measurement) final {
    absl::StatusOr<CheckinResult> result =
        MockCheckin(task_eligibility_info, attestation_measurement);
    network_stats_ += kCheckinPlanUriReceivedNetworkStats;
    if (result.ok() &&
        std::holds_alternative<FederatedProtocol::TaskAssignment>(*result)) {
      payload_uris_received_callback(
          std::get<FederatedProtocol::TaskAssignment>(*result));
      network_stats_ += kCheckinArtifactRetrievalNetworkStats;
    }
    retry_window_ = GetPostCheckinRetryWindow();
    return result;
  };
  MOCK_METHOD(absl::StatusOr<CheckinResult>, MockCheckin,
              (const std::optional<
                   ::google::internal::federatedml::v2::TaskEligibilityInfo>&
                   task_eligibility_info,
               std::optional<std::string> attestation_measurement));

  absl::StatusOr<MultipleTaskAssignments> PerformMultipleTaskAssignments(
      const std::vector<std::string>& task_names,
      const std::function<void(size_t)>& payload_uris_received_callback,
      const std::optional<std::string>& attestation_measurement) final {
    absl::StatusOr<MultipleTaskAssignments> result =
        MockPerformMultipleTaskAssignments(task_names,
                                           payload_uris_received_callback,
                                           attestation_measurement);
    network_stats_ += kMultipleTaskAssignmentsPlanUriReceivedNetworkStats;
    if (result.ok() && !result->task_assignments.empty()) {
      payload_uris_received_callback(result->task_assignments.size());
      network_stats_ += kMultipleTaskAssignmentsArtifactRetrievalNetworkStats;
    }
    return result;
  };

  MOCK_METHOD(
      absl::StatusOr<MultipleTaskAssignments>,
      MockPerformMultipleTaskAssignments,
      (const std::vector<std::string>& task_names,
       const std::function<void(size_t)>& payload_uris_received_callback,
       std::optional<std::string> attestation_measurement));

  absl::Status ReportCompleted(
      ComputationResults results, absl::Duration plan_duration,
      std::optional<std::string> aggregation_session_id,
      std::optional<confidentialcompute::PayloadMetadata> payload_metadata)
      final {
    network_stats_ += kReportCompletedNetworkStats;
    retry_window_ = GetPostReportCompletedRetryWindow();
    return MockReportCompleted(std::move(results), plan_duration,
                               aggregation_session_id, payload_metadata);
  };
  MOCK_METHOD(
      absl::Status, MockReportCompleted,
      (ComputationResults results, absl::Duration plan_duration,
       std::optional<std::string> aggregation_session_id,
       std::optional<confidentialcompute::PayloadMetadata> payload_metadata));

  absl::Status ReportNotCompleted(
      engine::PhaseOutcome phase_outcome, absl::Duration plan_duration,
      std::optional<std::string> aggregation_session_id) final {
    network_stats_ += kReportNotCompletedNetworkStats;
    retry_window_ = GetPostReportNotCompletedRetryWindow();
    return MockReportNotCompleted(phase_outcome, plan_duration,
                                  aggregation_session_id);
  };
  MOCK_METHOD(absl::Status, MockReportNotCompleted,
              (engine::PhaseOutcome phase_outcome, absl::Duration plan_duration,
               std::optional<std::string> aggregation_session_id));

  ::google::internal::federatedml::v2::RetryWindow GetLatestRetryWindow()
      final {
    return retry_window_;
  }

  NetworkStats GetNetworkStats() final { return network_stats_; }

 private:
  NetworkStats network_stats_;
  ::google::internal::federatedml::v2::RetryWindow retry_window_ =
      GetInitialRetryWindow();
};

class MockLogManager : public LogManager {
 public:
  MOCK_METHOD(void, LogDiag, (ProdDiagCode), (override));
  MOCK_METHOD(void, LogDiag, (DebugDiagCode), (override));
  MOCK_METHOD(void, LogToLongHistogram,
              (fcp::client::HistogramCounters, int, int,
               fcp::client::engine::DataSourceType, int64_t),
              (override));
  MOCK_METHOD(void, SetModelIdentifier, (const std::string&), (override));
};

class MockOpStatsLogger : public ::fcp::client::opstats::OpStatsLogger {
 public:
  MOCK_METHOD(
      void, AddEventAndSetTaskName,
      (const std::string& task_name,
       ::fcp::client::opstats::OperationalStats::Event::EventKind event),
      (override));
  MOCK_METHOD(
      void, AddEvent,
      (::fcp::client::opstats::OperationalStats::Event::EventKind event),
      (override));
  MOCK_METHOD(void, AddEventWithErrorMessage,
              (::fcp::client::opstats::OperationalStats::Event::EventKind event,
               const std::string& error_message),
              (override));
  MOCK_METHOD(void, RecordCollectionFirstAccessTime,
              (absl::string_view collection_uri, absl::Time first_access_time),
              (override));
  MOCK_METHOD(void, UpdateDatasetStats,
              (const std::string& collection_uri, int additional_example_count,
               int64_t additional_example_size_bytes),
              (override));
  MOCK_METHOD(void, SetNetworkStats, (const NetworkStats& network_stats),
              (override));
  MOCK_METHOD(void, SetMinSepPolicyIndex, (int64_t current_index), (override));
  MOCK_METHOD(void, SetRetryWindow,
              (google::internal::federatedml::v2::RetryWindow retry_window),
              (override));
  MOCK_METHOD(::fcp::client::opstats::OpStatsDb*, GetOpStatsDb, (), (override));
  MOCK_METHOD(absl::Status, CommitToStorage, (), (override));
  MOCK_METHOD(std::string, GetCurrentTaskName, (), (override));
  MOCK_METHOD(void, StartLoggingForPhase,
              (::fcp::client::opstats::OperationalStats::PhaseStats::Phase),
              (override));
  MOCK_METHOD(void, StopLoggingForTheCurrentPhase, (), (override));
};

class MockSimpleTaskEnvironment : public SimpleTaskEnvironment {
 public:
  MOCK_METHOD(std::string, GetBaseDir, (), (override));
  MOCK_METHOD(std::string, GetCacheDir, (), (override));
  MOCK_METHOD((absl::StatusOr<std::unique_ptr<ExampleIterator>>),
              CreateExampleIterator,
              (const google::internal::federated::plan::ExampleSelector&
                   example_selector),
              (override));
  MOCK_METHOD((absl::StatusOr<std::unique_ptr<ExampleIterator>>),
              CreateExampleIterator,
              (const google::internal::federated::plan::ExampleSelector&
                   example_selector,
               const SelectorContext& selector_context),
              (override));
  MOCK_METHOD(std::unique_ptr<fcp::client::http::HttpClient>, CreateHttpClient,
              (), (override));
  MOCK_METHOD(bool, TrainingConditionsSatisfied, (), (override));
  MOCK_METHOD(bool, OnTaskCompleted, (const TaskResultInfo& task_result_info),
              (override));
  MOCK_METHOD(absl::StatusOr<std::string>, GetAttestationMeasurement,
              (std::string content_binding), (override));
};

class MockExampleIterator : public ExampleIterator {
 public:
  MOCK_METHOD(absl::StatusOr<std::string>, Next, (), (override));
  MOCK_METHOD(void, Close, (), (override));
};

// An iterator that passes through each example in the dataset once.
class SimpleExampleIterator : public ExampleIterator {
 public:
  // Uses the given bytes as the examples to return.
  explicit SimpleExampleIterator(std::vector<const char*> examples);
  // Passes through each of the examples in the `Dataset.client_data.example`
  // field.
  explicit SimpleExampleIterator(
      google::internal::federated::plan::Dataset dataset);
  // Uses the given strings as the examples to return.
  explicit SimpleExampleIterator(const std::vector<std::string>& examples);
  // Passes through each of the examples in the
  // `Dataset.client_data.selected_example.example` field, whose example
  // collection URI matches the provided `collection_uri`.
  SimpleExampleIterator(google::internal::federated::plan::Dataset dataset,
                        absl::string_view collection_uri);
  absl::StatusOr<std::string> Next() override;
  void Close() override {}

 protected:
  std::vector<std::string> examples_;
  int index_ = 0;
};

struct ComputationArtifacts {
  // The path to the file containing the plan data.
  std::string plan_filepath;
  // The already-parsed plan data.
  google::internal::federated::plan::ClientOnlyPlan plan;
  // The test dataset.
  google::internal::federated::plan::Dataset dataset;
  // The path to the file containing the initial checkpoint data (not set for
  // local compute task artifacts).
  std::string checkpoint_filepath;
  // The initial checkpoint data, as a string (not set for local compute task
  // artifacts).
  std::string checkpoint;
  // The Federated Select slice data (not set for local compute task artifacts).
  google::internal::federated::plan::SlicesTestDataset federated_select_slices;
  // A fake PopulationEligibilitySpec matching the policies in the
  // EligibilityEvalTask (only non-empty for EET artifacts)
  google::internal::federated::plan::PopulationEligibilitySpec
      population_eligibility_spec;
};

class MockFlags : public Flags {
 public:
  MOCK_METHOD(int64_t, condition_polling_period_millis, (), (const, override));
  MOCK_METHOD(int64_t, tf_execution_teardown_grace_period_millis, (),
              (const, override));
  MOCK_METHOD(int64_t, tf_execution_teardown_extended_period_millis, (),
              (const, override));
  MOCK_METHOD(bool, log_tensorflow_error_messages, (), (const, override));
  MOCK_METHOD(int64_t, opstats_ttl_days, (), (const, override));
  MOCK_METHOD(int64_t, opstats_db_size_limit_bytes, (), (const, override));
  MOCK_METHOD(int64_t, federated_training_transient_errors_retry_delay_secs, (),
              (const, override));
  MOCK_METHOD(float,
              federated_training_transient_errors_retry_delay_jitter_percent,
              (), (const, override));
  MOCK_METHOD(int64_t, federated_training_permanent_errors_retry_delay_secs, (),
              (const, override));
  MOCK_METHOD(float,
              federated_training_permanent_errors_retry_delay_jitter_percent,
              (), (const, override));
  MOCK_METHOD(std::vector<int32_t>, federated_training_permanent_error_codes,
              (), (const, override));
  MOCK_METHOD(bool, ensure_dynamic_tensors_are_released, (), (const, override));
  MOCK_METHOD(int32_t, large_tensor_threshold_for_dynamic_allocation, (),
              (const, override));
  MOCK_METHOD(bool, disable_http_request_body_compression, (),
              (const, override));
  MOCK_METHOD(bool, enable_computation_id, (), (const, override));
  MOCK_METHOD(int32_t, waiting_period_sec_for_cancellation, (),
              (const, override));
  MOCK_METHOD(bool, enable_federated_select, (), (const, override));
  MOCK_METHOD(int32_t, num_threads_for_tflite, (), (const, override));
  MOCK_METHOD(bool, disable_tflite_delegate_clustering, (), (const, override));
  MOCK_METHOD(bool, enable_lightweight_client_report_wire_format, (),
              (const, override));
  MOCK_METHOD(bool, enable_confidential_aggregation, (), (const, override));
  MOCK_METHOD(bool, check_trustworthiness_for_min_sep_policy, (),
              (const, override));
  MOCK_METHOD(bool, enable_direct_data_upload_task, (), (const, override));
  MOCK_METHOD(bool, enable_relative_uri_prefix, (), (const, override));
  MOCK_METHOD(bool, log_min_sep_index_to_phase_stats, (), (const, override));
  MOCK_METHOD(int32_t, http_retry_max_attempts, (), (const, override));
  MOCK_METHOD(bool, enable_event_time_data_upload, (), (const, override));
  MOCK_METHOD(bool, enable_blob_header_in_http_headers, (), (const, override));
  MOCK_METHOD(bool, move_device_attestation_to_start_task_assignment, (),
              (const, override));
  MOCK_METHOD(bool, enable_privacy_id_generation, (), (const, override));
};

// Helper methods for extracting opstats fields from TF examples.
std::string ExtractSingleString(const tensorflow::Example& example,
                                const char key[]);
google::protobuf::RepeatedPtrField<std::string> ExtractRepeatedString(
    const tensorflow::Example& example, const char key[]);
int64_t ExtractSingleInt64(const tensorflow::Example& example,
                           const char key[]);
google::protobuf::RepeatedField<int64_t> ExtractRepeatedInt64(
    const tensorflow::Example& example, const char key[]);

class MockOpStatsDb : public ::fcp::client::opstats::OpStatsDb {
 public:
  MOCK_METHOD(absl::StatusOr<::fcp::client::opstats::OpStatsSequence>, Read, (),
              (override));
  MOCK_METHOD(absl::Status, Transform,
              (std::function<void(::fcp::client::opstats::OpStatsSequence&)>),
              (override));
};

class MockPhaseLogger : public PhaseLogger {
 public:
  MOCK_METHOD(
      void, UpdateRetryWindowAndNetworkStats,
      (const ::google::internal::federatedml::v2::RetryWindow& retry_window,
       const NetworkStats& network_stats),
      (override));
  MOCK_METHOD(void, SetModelIdentifier, (absl::string_view model_identifier),
              (override));
  MOCK_METHOD(void, LogTaskNotStarted, (absl::string_view error_message),
              (override));
  MOCK_METHOD(void, LogNonfatalInitializationError, (absl::Status error_status),
              (override));
  MOCK_METHOD(void, LogFatalInitializationError, (absl::Status error_status),
              (override));
  MOCK_METHOD(void, LogEligibilityEvalCheckinStarted, (), (override));
  MOCK_METHOD(void, LogEligibilityEvalCheckinIOError,
              (absl::Status error_status, const NetworkStats& network_stats,
               absl::Time time_before_checkin),
              (override));
  MOCK_METHOD(void, LogEligibilityEvalCheckinInvalidPayloadError,
              (absl::string_view error_message,
               const NetworkStats& network_stats,
               absl::Time time_before_checkin),
              (override));
  MOCK_METHOD(void, LogEligibilityEvalCheckinClientInterrupted,
              (absl::Status error_status, const NetworkStats& network_stats,
               absl::Time time_before_checkin),
              (override));
  MOCK_METHOD(void, LogEligibilityEvalCheckinServerAborted,
              (absl::Status error_status, const NetworkStats& network_stats,
               absl::Time time_before_checkin),
              (override));
  MOCK_METHOD(void, LogEligibilityEvalNotConfigured,
              (const NetworkStats& network_stats,
               absl::Time time_before_checkin),
              (override));
  MOCK_METHOD(void, LogEligibilityEvalCheckinTurnedAway,
              (const NetworkStats& network_stats,
               absl::Time time_before_checkin),
              (override));
  MOCK_METHOD(void, LogEligibilityEvalCheckinPlanUriReceived,
              (const NetworkStats& network_stats,
               absl::Time time_before_checkin),
              (override));
  MOCK_METHOD(void, LogEligibilityEvalCheckinCompleted,
              (const NetworkStats& network_stats,
               absl::Time time_before_checkin,
               absl::Time time_before_plan_download),
              (override));
  MOCK_METHOD(void, LogEligibilityEvalComputationStarted, (), (override));
  MOCK_METHOD(void, LogEligibilityEvalComputationInvalidArgument,
              (absl::Status error_status, const ExampleStats& example_stats,
               absl::Time run_plan_start_time),
              (override));
  MOCK_METHOD(void, LogEligibilityEvalComputationExampleIteratorError,
              (absl::Status error_status, const ExampleStats& example_stats,
               absl::Time run_plan_start_time),
              (override));
  MOCK_METHOD(void, LogEligibilityEvalComputationTensorflowError,
              (absl::Status error_status, const ExampleStats& example_stats,
               absl::Time run_plan_start_time, absl::Time reference_time),
              (override));
  MOCK_METHOD(void, LogEligibilityEvalComputationIOError,
              (absl::Status error_status, const ExampleStats& example_stats,
               absl::Time run_plan_start_time, absl::Time reference_time),
              (override));
  MOCK_METHOD(void, LogEligibilityEvalComputationInterrupted,
              (absl::Status error_status, const ExampleStats& example_stats,
               absl::Time run_plan_start_time, absl::Time reference_time),
              (override));
  MOCK_METHOD(void, LogEligibilityEvalComputationErrorNonfatal,
              (absl::Status error_status), (override));
  MOCK_METHOD(void, LogEligibilityEvalComputationCompleted,
              (const ExampleStats& example_stats,
               absl::Time run_plan_start_time, absl::Time reference_time),
              (override));
  MOCK_METHOD(void, LogMultipleTaskAssignmentsStarted, (), (override));
  MOCK_METHOD(void, LogMultipleTaskAssignmentsIOError,
              (absl::Status error_status, const NetworkStats& network_stats,
               absl::Time time_before_multiple_task_assignments,
               absl::Time reference_time),
              (override));
  MOCK_METHOD(void, LogMultipleTaskAssignmentsPayloadIOError,
              (absl::Status error_status), (override));
  MOCK_METHOD(void, LogMultipleTaskAssignmentsInvalidPayload,
              (absl::string_view error_message), (override));
  MOCK_METHOD(void, LogMultipleTaskAssignmentsClientInterrupted,
              (absl::Status error_status, const NetworkStats& network_stats,
               absl::Time time_before_multiple_task_assignments,
               absl::Time reference_time),
              (override));
  MOCK_METHOD(void, LogMultipleTaskAssignmentsServerAborted,
              (absl::Status error_status, const NetworkStats& network_stats,
               absl::Time time_before_multiple_task_assignments,
               absl::Time reference_time),
              (override));
  MOCK_METHOD(void, LogMultipleTaskAssignmentsTurnedAway,
              (const NetworkStats& network_stats,
               absl::Time time_before_multiple_task_assignments,
               absl::Time reference_time),
              (override));
  MOCK_METHOD(void, LogMultipleTaskAssignmentsPlanUriReceived,
              (const NetworkStats& network_stats,
               absl::Time time_before_multiple_task_assignments),
              (override));
  MOCK_METHOD(void, LogMultipleTaskAssignmentsPlanUriPartialReceived,
              (const NetworkStats& network_stats,
               absl::Time time_before_multiple_task_assignments),
              (override));
  MOCK_METHOD(void, LogMultipleTaskAssignmentsPartialCompleted,
              (const NetworkStats& network_stats,
               absl::Time time_before_multiple_task_assignments,
               absl::Time time_before_plan_download, absl::Time reference_time),
              (override));
  MOCK_METHOD(void, LogMultipleTaskAssignmentsCompleted,
              (const NetworkStats& network_stats,
               absl::Time time_before_multiple_task_assignments,
               absl::Time time_before_plan_download, absl::Time reference_time),
              (override));
  MOCK_METHOD(void, LogCheckinStarted, (), (override));
  MOCK_METHOD(void, LogCheckinIOError,
              (absl::Status error_status, const NetworkStats& network_stats,
               absl::Time time_before_checkin, absl::Time reference_time),
              (override));
  MOCK_METHOD(void, LogCheckinInvalidPayload,
              (absl::string_view error_message,
               const NetworkStats& network_stats,
               absl::Time time_before_checkin, absl::Time reference_time),
              (override));
  MOCK_METHOD(void, LogCheckinClientInterrupted,
              (absl::Status error_status, const NetworkStats& network_stats,
               absl::Time time_before_checkin, absl::Time reference_time),
              (override));
  MOCK_METHOD(void, LogCheckinServerAborted,
              (absl::Status error_status, const NetworkStats& network_stats,
               absl::Time time_before_checkin, absl::Time reference_time),
              (override));
  MOCK_METHOD(void, LogCheckinTurnedAway,
              (const NetworkStats& network_stats,
               absl::Time time_before_checkin, absl::Time reference_time),
              (override));
  MOCK_METHOD(void, LogCheckinPlanUriReceived,
              (absl::string_view task_name, const NetworkStats& network_stats,
               absl::Time time_before_checkin),
              (override));
  MOCK_METHOD(void, LogCheckinCompleted,
              (absl::string_view task_name, const NetworkStats& network_stats,
               absl::Time time_before_checkin,
               absl::Time time_before_plan_download, absl::Time reference_time),
              (override));
  MOCK_METHOD(void, MaybeLogCollectionFirstAccessTime, (absl::string_view),
              (override));
  MOCK_METHOD(void, LogComputationStarted, (absl::string_view), (override));
  MOCK_METHOD(void, LogComputationInvalidArgument,
              (absl::Status error_status, const ExampleStats& example_stats,
               const NetworkStats& network_stats,
               absl::Time run_plan_start_time),
              (override));
  MOCK_METHOD(void, LogComputationExampleIteratorError,
              (absl::Status error_status, const ExampleStats& example_stats,
               const NetworkStats& network_stats,
               absl::Time run_plan_start_time),
              (override));
  MOCK_METHOD(void, LogComputationIOError,
              (absl::Status error_status, const ExampleStats& example_stats,
               const NetworkStats& network_stats,
               absl::Time run_plan_start_time),
              (override));
  MOCK_METHOD(void, LogComputationTensorflowError,
              (absl::Status error_status, const ExampleStats& example_stats,
               const NetworkStats& network_stats,
               absl::Time run_plan_start_time, absl::Time reference_time),
              (override));
  MOCK_METHOD(void, LogComputationInterrupted,
              (absl::Status error_status, const ExampleStats& example_stats,
               const NetworkStats& network_stats,
               absl::Time run_plan_start_time, absl::Time reference_time),
              (override));
  MOCK_METHOD(void, LogComputationCompleted,
              (const ExampleStats& example_stats,
               const NetworkStats& network_stats,
               absl::Time run_plan_start_time, absl::Time reference_time,
               (std::optional<int64_t> min_sep_policy_index)),
              (override));
  MOCK_METHOD(absl::Status, LogResultUploadStarted, (), (override));
  MOCK_METHOD(void, LogResultUploadIOError,
              (absl::Status error_status, const NetworkStats& network_stats,
               absl::Time time_before_result_upload, absl::Time reference_time),
              (override));
  MOCK_METHOD(void, LogResultUploadClientInterrupted,
              (absl::Status error_status, const NetworkStats& network_stats,
               absl::Time time_before_result_upload, absl::Time reference_time),
              (override));
  MOCK_METHOD(void, LogResultUploadServerAborted,
              (absl::Status error_status, const NetworkStats& network_stats,
               absl::Time time_before_result_upload, absl::Time reference_time),
              (override));
  MOCK_METHOD(void, LogResultUploadCompleted,
              (const NetworkStats& network_stats,
               absl::Time time_before_result_upload, absl::Time reference_time),
              (override));
  MOCK_METHOD(absl::Status, LogFailureUploadStarted, (), (override));
  MOCK_METHOD(void, LogFailureUploadIOError,
              (absl::Status error_status, const NetworkStats& network_stats,
               absl::Time time_before_failure_upload,
               absl::Time reference_time),
              (override));
  MOCK_METHOD(void, LogFailureUploadClientInterrupted,
              (absl::Status error_status, const NetworkStats& network_stats,
               absl::Time time_before_failure_upload,
               absl::Time reference_time),
              (override));
  MOCK_METHOD(void, LogFailureUploadServerAborted,
              (absl::Status error_status, const NetworkStats& network_stats,
               absl::Time time_before_failure_upload,
               absl::Time reference_time),
              (override));
  MOCK_METHOD(void, LogFailureUploadCompleted,
              (const NetworkStats& network_stats,
               absl::Time time_before_result_upload, absl::Time reference_time),
              (override));
};

class MockFederatedSelectManager : public FederatedSelectManager {
 public:
  MOCK_METHOD(std::unique_ptr<engine::ExampleIteratorFactory>,
              CreateExampleIteratorFactoryForUriTemplate,
              (absl::string_view uri_template), (override));

  MOCK_METHOD(NetworkStats, GetNetworkStats, (), (override));
};

class MockFederatedSelectExampleIteratorFactory
    : public FederatedSelectExampleIteratorFactory {
 public:
  MOCK_METHOD(absl::StatusOr<std::unique_ptr<ExampleIterator>>,
              CreateExampleIterator,
              (const ::google::internal::federated::plan::ExampleSelector&
                   example_selector),
              (override));
};

class MockSecAggRunnerFactory : public SecAggRunnerFactory {
 public:
  MOCK_METHOD(std::unique_ptr<SecAggRunner>, CreateSecAggRunner,
              (std::unique_ptr<SecAggSendToServerBase> send_to_server_impl,
               std::unique_ptr<SecAggProtocolDelegate> protocol_delegate,
               SecAggEventPublisher* secagg_event_publisher,
               LogManager* log_manager,
               InterruptibleRunner* interruptible_runner,
               int64_t expected_number_of_clients,
               int64_t minimum_surviving_clients_for_reconstruction),
              (override));
};

class MockSecAggRunner : public SecAggRunner {
 public:
  MOCK_METHOD(absl::Status, Run, (ComputationResults results), (override));
};

class MockSecAggSendToServerBase : public SecAggSendToServerBase {
  MOCK_METHOD(void, Send, (secagg::ClientToServerWrapperMessage * message),
              (override));
};

class MockSecAggProtocolDelegate : public SecAggProtocolDelegate {
 public:
  MOCK_METHOD(absl::StatusOr<uint64_t>, GetModulus, (const std::string& key),
              (override));
  MOCK_METHOD(absl::StatusOr<secagg::ServerToClientWrapperMessage>,
              ReceiveServerMessage, (), (override));
  MOCK_METHOD(void, Abort, (), (override));
};

class MockAttestationVerifier : public attestation::AttestationVerifier {
 public:
  MOCK_METHOD(
      absl::StatusOr<VerificationResult>, Verify,
      (const absl::Cord& access_policy,
       const confidentialcompute::SignedEndorsements& signed_endorsements,
       const google::internal::federatedcompute::v1::
           ConfidentialEncryptionConfig& encryption_config),
      (override));
};

class MockTensorflowRunner : public TensorflowRunner {
 public:
  MOCK_METHOD(
      engine::PlanResult, RunEligibilityEvalPlanWithTensorflowSpec,
      (std::vector<engine::ExampleIteratorFactory*> example_iterator_factories,
       std::function<bool()> should_abort, LogManager* log_manager,
       opstats::OpStatsLogger* opstats_logger, const Flags* flags,
       const google::internal::federated::plan::ClientOnlyPlan& client_plan,
       const std::string& checkpoint_input_filename,
       const fcp::client::InterruptibleRunner::TimingConfig& timing_config,
       absl::Time run_plan_start_time, absl::Time reference_time),
      (override));
  MOCK_METHOD(
      PlanResultAndCheckpointFile, RunPlanWithTensorflowSpec,
      (std::vector<engine::ExampleIteratorFactory*> example_iterator_factories,
       std::function<bool()> should_abort, LogManager* log_manager,
       opstats::OpStatsLogger* opstats_logger, const Flags* flags,
       ExampleIteratorQueryRecorder* example_iterator_query_recorder,
       const google::internal::federated::plan::ClientOnlyPlan& client_plan,
       const std::string& checkpoint_input_filename,
       const std::string& checkpoint_output_filename,
       const fcp::client::InterruptibleRunner::TimingConfig& timing_config),
      (override));
  MOCK_METHOD(
      absl::Status, WriteTFV1Checkpoint,
      (const std::string& output_checkpoint_filename,
       (const std::vector<std::pair<
            google::internal::federated::plan::ExampleQuerySpec::ExampleQuery,
            ExampleQueryResult>>& example_query_results)),
      (override));
};

// A TensorflowRunner that delegates all calls to a MockTensorflowRunner.
class TestingTensorflowRunner : public TensorflowRunner {
 public:
  explicit TestingTensorflowRunner(MockTensorflowRunner* delegate)
      : delegate_(*delegate) {}
  engine::PlanResult RunEligibilityEvalPlanWithTensorflowSpec(
      std::vector<engine::ExampleIteratorFactory*> example_iterator_factories,
      std::function<bool()> should_abort, LogManager* log_manager,
      opstats::OpStatsLogger* opstats_logger, const Flags* flags,
      const google::internal::federated::plan::ClientOnlyPlan& client_plan,
      const std::string& checkpoint_input_filename,
      const fcp::client::InterruptibleRunner::TimingConfig& timing_config,
      absl::Time run_plan_start_time, absl::Time reference_time) override;

  PlanResultAndCheckpointFile RunPlanWithTensorflowSpec(
      std::vector<engine::ExampleIteratorFactory*> example_iterator_factories,
      std::function<bool()> should_abort, LogManager* log_manager,
      opstats::OpStatsLogger* opstats_logger, const Flags* flags,
      ExampleIteratorQueryRecorder* example_iterator_query_recorder,
      const google::internal::federated::plan::ClientOnlyPlan& client_plan,
      const std::string& checkpoint_input_filename,
      const std::string& checkpoint_output_filename,
      const fcp::client::InterruptibleRunner::TimingConfig& timing_config)
      override;

  absl::Status WriteTFV1Checkpoint(
      const std::string& output_checkpoint_filename,
      const std::vector<std::pair<
          google::internal::federated::plan::ExampleQuerySpec::ExampleQuery,
          ExampleQueryResult>>& example_query_results) override;

 private:
  MockTensorflowRunner& delegate_;
};

}  // namespace client
}  // namespace fcp

#endif  // FCP_CLIENT_TEST_HELPERS_H_
