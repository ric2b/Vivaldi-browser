/*
 * Copyright 2022 Google LLC
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
#include "fcp/client/phase_logger_impl.h"

#include <cstdint>
#include <optional>
#include <string>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "fcp/base/monitoring.h"
#include "fcp/client/diag_codes.pb.h"
#include "fcp/client/histogram_counters.pb.h"
#include "fcp/client/stats.h"
#include "fcp/protos/opstats.pb.h"

namespace fcp {
namespace client {
namespace {
constexpr absl::string_view kInitializationErrorPrefix =
    "Error during initialization: ";
constexpr absl::string_view kEligibilityCheckinErrorPrefix =
    "Error during eligibility check-in: ";
constexpr absl::string_view kEligibilityComputationErrorPrefix =
    "Error during eligibility eval computation: ";
constexpr absl::string_view kMultipleTaskAssignmentsPrefix =
    "Error during multiple task assignments: ";
constexpr absl::string_view kCheckinErrorPrefix = "Error during check-in: ";
constexpr absl::string_view kComputationErrorPrefix =
    "Error during computation: ";
constexpr absl::string_view kResultUploadErrorPrefix =
    "Error reporting results: ";
constexpr absl::string_view kFailureUploadErrorPrefix =
    "Error reporting computation failure: ";
}  // anonymous namespace

using ::fcp::client::opstats::OperationalStats;
using ::google::internal::federatedml::v2::RetryWindow;

void PhaseLoggerImpl::UpdateRetryWindowAndNetworkStats(
    const RetryWindow& retry_window, const NetworkStats& network_stats) {
  opstats_logger_->SetRetryWindow(retry_window);

  // Update the network stats.
  opstats_logger_->SetNetworkStats(network_stats);
}

void PhaseLoggerImpl::SetModelIdentifier(absl::string_view model_identifier) {
  event_publisher_->SetModelIdentifier(std::string(model_identifier));
  log_manager_->SetModelIdentifier(std::string(model_identifier));
}

void PhaseLoggerImpl::LogTaskNotStarted(absl::string_view error_message) {
  event_publisher_->PublishTaskNotStarted(error_message);
  opstats_logger_->AddEventWithErrorMessage(
      OperationalStats::Event::EVENT_KIND_TRAIN_NOT_STARTED,
      std::string(error_message));
}

void PhaseLoggerImpl::LogNonfatalInitializationError(
    absl::Status error_status) {
  std::string error_message = GetErrorMessage(
      error_status, kInitializationErrorPrefix, /* keep_error_message= */ true);
  event_publisher_->PublishNonfatalInitializationError(error_message);
  opstats_logger_->AddEventWithErrorMessage(
      OperationalStats::Event::EVENT_KIND_INITIALIZATION_ERROR_FATAL,
      error_message);
}

void PhaseLoggerImpl::LogFatalInitializationError(absl::Status error_status) {
  std::string error_message = GetErrorMessage(
      error_status, kInitializationErrorPrefix, /* keep_error_message= */ true);
  event_publisher_->PublishFatalInitializationError(error_message);
  opstats_logger_->AddEventWithErrorMessage(
      OperationalStats::Event::EVENT_KIND_INITIALIZATION_ERROR_NONFATAL,
      error_message);
}

void PhaseLoggerImpl::LogEligibilityEvalCheckinStarted() {
  event_publisher_->PublishEligibilityEvalCheckin();
  opstats_logger_->StartLoggingForPhase(
      OperationalStats::PhaseStats::ELIGIBILITY_EVAL_CHECKIN);
  opstats_logger_->AddEvent(
      OperationalStats::Event::EVENT_KIND_ELIGIBILITY_CHECKIN_STARTED);
}

void PhaseLoggerImpl::LogEligibilityEvalCheckinIOError(
    absl::Status error_status, const NetworkStats& network_stats,
    absl::Time time_before_checkin) {
  std::string error_message =
      GetErrorMessage(error_status, kEligibilityCheckinErrorPrefix,
                      /* keep_error_message= */ true);
  event_publisher_->PublishEligibilityEvalCheckinIoError(
      error_message, network_stats, absl::Now() - time_before_checkin);
  opstats_logger_->AddEventWithErrorMessage(
      OperationalStats::Event::EVENT_KIND_ELIGIBILITY_CHECKIN_ERROR_IO,
      error_message);
  opstats_logger_->StopLoggingForTheCurrentPhase();
  LogEligibilityEvalCheckinLatency(time_before_checkin);
}

void PhaseLoggerImpl::LogEligibilityEvalCheckinClientInterrupted(
    absl::Status error_status, const NetworkStats& network_stats,
    absl::Time time_before_checkin) {
  std::string error_message =
      GetErrorMessage(error_status, kEligibilityCheckinErrorPrefix,
                      /* keep_error_message= */ true);
  event_publisher_->PublishEligibilityEvalCheckinClientInterrupted(
      error_message, network_stats, absl::Now() - time_before_checkin);
  opstats_logger_->AddEventWithErrorMessage(
      OperationalStats::Event::
          EVENT_KIND_ELIGIBILITY_CHECKIN_CLIENT_INTERRUPTED,
      error_message);
  opstats_logger_->StopLoggingForTheCurrentPhase();
  LogEligibilityEvalCheckinLatency(time_before_checkin);
}

void PhaseLoggerImpl::LogEligibilityEvalCheckinServerAborted(
    absl::Status error_status, const NetworkStats& network_stats,
    absl::Time time_before_checkin) {
  std::string error_message =
      GetErrorMessage(error_status, kEligibilityCheckinErrorPrefix,
                      /* keep_error_message= */ true);
  event_publisher_->PublishEligibilityEvalCheckinServerAborted(
      error_message, network_stats, absl::Now() - time_before_checkin);
  opstats_logger_->AddEventWithErrorMessage(
      OperationalStats::Event::EVENT_KIND_ELIGIBILITY_CHECKIN_SERVER_ABORTED,
      error_message);
  opstats_logger_->StopLoggingForTheCurrentPhase();
  LogEligibilityEvalCheckinLatency(time_before_checkin);
}

void PhaseLoggerImpl::LogEligibilityEvalNotConfigured(
    const NetworkStats& network_stats, absl::Time time_before_checkin) {
  event_publisher_->PublishEligibilityEvalNotConfigured(
      network_stats, absl::Now() - time_before_checkin);
  opstats_logger_->AddEvent(
      OperationalStats::Event::EVENT_KIND_ELIGIBILITY_DISABLED);
  opstats_logger_->StopLoggingForTheCurrentPhase();
  LogEligibilityEvalCheckinLatency(time_before_checkin);
}

void PhaseLoggerImpl::LogEligibilityEvalCheckinTurnedAway(
    const NetworkStats& network_stats, absl::Time time_before_checkin) {
  event_publisher_->PublishEligibilityEvalRejected(
      network_stats, absl::Now() - time_before_checkin);
  opstats_logger_->AddEvent(
      OperationalStats::Event::EVENT_KIND_ELIGIBILITY_REJECTED);
  opstats_logger_->StopLoggingForTheCurrentPhase();
  LogEligibilityEvalCheckinLatency(time_before_checkin);
}

void PhaseLoggerImpl::LogEligibilityEvalCheckinInvalidPayloadError(
    absl::string_view error_message, const NetworkStats& network_stats,
    absl::Time time_before_checkin) {
  log_manager_->LogDiag(
      ProdDiagCode::
          BACKGROUND_TRAINING_ELIGIBILITY_EVAL_FAILED_CANNOT_PARSE_PLAN);
  event_publisher_->PublishEligibilityEvalCheckinErrorInvalidPayload(
      error_message, network_stats, absl::Now() - time_before_checkin);
  opstats_logger_->AddEventWithErrorMessage(
      OperationalStats::Event::
          EVENT_KIND_ELIGIBILITY_CHECKIN_ERROR_INVALID_PAYLOAD,
      std::string(error_message));
  opstats_logger_->StopLoggingForTheCurrentPhase();
  LogEligibilityEvalCheckinLatency(time_before_checkin);
}

void PhaseLoggerImpl::LogEligibilityEvalCheckinPlanUriReceived(
    const NetworkStats& network_stats, absl::Time time_before_checkin) {
  opstats_logger_->AddEvent(
      OperationalStats::Event::EVENT_KIND_ELIGIBILITY_PLAN_URI_RECEIVED);
  event_publisher_->PublishEligibilityEvalPlanUriReceived(
      network_stats, absl::Now() - time_before_checkin);
}

void PhaseLoggerImpl::LogEligibilityEvalCheckinCompleted(
    const NetworkStats& network_stats, absl::Time time_before_checkin,
    absl::Time time_before_plan_download) {
  opstats_logger_->AddEvent(
      OperationalStats::Event::EVENT_KIND_ELIGIBILITY_ENABLED);
  opstats_logger_->StopLoggingForTheCurrentPhase();
  absl::Time before_time = time_before_plan_download;
  event_publisher_->PublishEligibilityEvalPlanReceived(
      network_stats, absl::Now() - before_time);

  // The 'EligibilityEvalCheckinLatency' should cover the whole period from
  // eligibility eval checkin to completion (and not just the period from EET
  // plan URIs being received to completion).
  LogEligibilityEvalCheckinLatency(time_before_checkin);
}

void PhaseLoggerImpl::LogEligibilityEvalComputationStarted() {
  event_publisher_->PublishEligibilityEvalComputationStarted();
  opstats_logger_->StartLoggingForPhase(
      OperationalStats::PhaseStats::ELIGIBILITY_COMPUTATION);
  opstats_logger_->AddEvent(
      OperationalStats::Event::EVENT_KIND_ELIGIBILITY_COMPUTATION_STARTED);
}

void PhaseLoggerImpl::LogEligibilityEvalComputationInvalidArgument(
    absl::Status error_status, const ExampleStats& example_stats,
    absl::Time run_plan_start_time) {
  std::string error_message =
      GetErrorMessage(error_status, kEligibilityComputationErrorPrefix,
                      /* keep_error_message= */ true);
  log_manager_->LogDiag(
      ProdDiagCode::BACKGROUND_TRAINING_FAILED_PLAN_FAILS_SANITY_CHECK);
  event_publisher_->PublishEligibilityEvalComputationInvalidArgument(
      error_message, example_stats, absl::Now() - run_plan_start_time);
  opstats_logger_->AddEventWithErrorMessage(
      OperationalStats::Event::
          EVENT_KIND_ELIGIBILITY_COMPUTATION_ERROR_INVALID_ARGUMENT,
      error_message);
  opstats_logger_->StopLoggingForTheCurrentPhase();
}

void PhaseLoggerImpl::LogEligibilityEvalComputationExampleIteratorError(
    absl::Status error_status, const ExampleStats& example_stats,
    absl::Time run_plan_start_time) {
  std::string error_message =
      GetErrorMessage(error_status, kEligibilityComputationErrorPrefix,
                      /* keep_error_message= */ true);
  event_publisher_->PublishEligibilityEvalComputationExampleIteratorError(
      error_message, example_stats, absl::Now() - run_plan_start_time);
  opstats_logger_->AddEventWithErrorMessage(
      OperationalStats::Event::
          EVENT_KIND_ELIGIBILITY_COMPUTATION_ERROR_EXAMPLE_ITERATOR,
      error_message);
  opstats_logger_->StopLoggingForTheCurrentPhase();
}

void PhaseLoggerImpl::LogEligibilityEvalComputationTensorflowError(
    absl::Status error_status, const ExampleStats& example_stats,
    absl::Time run_plan_start_time, absl::Time reference_time) {
  std::string error_message =
      GetErrorMessage(error_status, kEligibilityComputationErrorPrefix,
                      log_tensorflow_error_messages_);
  event_publisher_->PublishEligibilityEvalComputationTensorflowError(
      error_message, example_stats, absl::Now() - run_plan_start_time);
  opstats_logger_->AddEventWithErrorMessage(
      OperationalStats::Event::
          EVENT_KIND_ELIGIBILITY_COMPUTATION_ERROR_TENSORFLOW,
      error_message);
  opstats_logger_->StopLoggingForTheCurrentPhase();
  LogEligibilityEvalComputationLatency(run_plan_start_time, reference_time);
}

void PhaseLoggerImpl::LogEligibilityEvalComputationIOError(
    absl::Status error_status, const ExampleStats& example_stats,
    absl::Time run_plan_start_time, absl::Time reference_time) {
  std::string error_message =
      GetErrorMessage(error_status, kEligibilityComputationErrorPrefix,
                      /* keep_error_message= */ true);
  event_publisher_->PublishEligibilityEvalComputationIOError(
      error_message, example_stats, absl::Now() - run_plan_start_time);
  opstats_logger_->AddEventWithErrorMessage(
      OperationalStats::Event::EVENT_KIND_ELIGIBILITY_COMPUTATION_ERROR_IO,
      error_message);
  opstats_logger_->StopLoggingForTheCurrentPhase();
  LogEligibilityEvalComputationLatency(run_plan_start_time, reference_time);
}

void PhaseLoggerImpl::LogEligibilityEvalComputationInterrupted(
    absl::Status error_status, const ExampleStats& example_stats,
    absl::Time run_plan_start_time, absl::Time reference_time) {
  std::string error_message =
      GetErrorMessage(error_status, kEligibilityComputationErrorPrefix,
                      /* keep_error_message= */ true);
  event_publisher_->PublishEligibilityEvalComputationInterrupted(
      error_message, example_stats, absl::Now() - run_plan_start_time);
  opstats_logger_->AddEventWithErrorMessage(
      OperationalStats::Event::
          EVENT_KIND_ELIGIBILITY_COMPUTATION_CLIENT_INTERRUPTED,
      error_message);
  opstats_logger_->StopLoggingForTheCurrentPhase();
  LogEligibilityEvalComputationLatency(run_plan_start_time, reference_time);
}

void PhaseLoggerImpl::LogEligibilityEvalComputationErrorNonfatal(
    absl::Status error_status) {
  std::string error_message =
      GetErrorMessage(error_status, kEligibilityComputationErrorPrefix,
                      /* keep_error_message= */ true);
  event_publisher_->PublishEligibilityEvalComputationErrorNonfatal(
      error_message);
}

void PhaseLoggerImpl::LogEligibilityEvalComputationCompleted(
    const ExampleStats& example_stats, absl::Time run_plan_start_time,
    absl::Time reference_time) {
  event_publisher_->PublishEligibilityEvalComputationCompleted(
      example_stats, absl::Now() - run_plan_start_time);
  opstats_logger_->AddEvent(
      OperationalStats::Event::EVENT_KIND_ELIGIBILITY_COMPUTATION_FINISHED);
  opstats_logger_->StopLoggingForTheCurrentPhase();
  log_manager_->LogToLongHistogram(
      HistogramCounters::TRAINING_OVERALL_EXAMPLE_SIZE,
      example_stats.example_size_bytes);
  log_manager_->LogToLongHistogram(
      HistogramCounters::TRAINING_OVERALL_EXAMPLE_COUNT,
      example_stats.example_count);
  LogEligibilityEvalComputationLatency(run_plan_start_time, reference_time);
}

void PhaseLoggerImpl::LogMultipleTaskAssignmentsStarted() {
  // Log that we are about to check in with the server.
  event_publisher_->PublishMultipleTaskAssignmentsStarted();
  opstats_logger_->StartLoggingForPhase(
      OperationalStats::PhaseStats::MULTIPLE_TASK_ASSIGNMENTS);
  opstats_logger_->AddEvent(
      OperationalStats::Event::EVENT_KIND_MULTIPLE_TASK_ASSIGNMENTS_STARTED);
}

void PhaseLoggerImpl::LogMultipleTaskAssignmentsIOError(
    absl::Status error_status, const NetworkStats& network_stats,
    absl::Time time_before_multiple_task_assignments,
    absl::Time reference_time) {
  std::string error_message =
      GetErrorMessage(error_status, kMultipleTaskAssignmentsPrefix,
                      /* keep_error_message= */ true);
  event_publisher_->PublishMultipleTaskAssignmentsIOError(
      error_message, network_stats,
      absl::Now() - time_before_multiple_task_assignments);
  opstats_logger_->AddEventWithErrorMessage(
      OperationalStats::Event::EVENT_KIND_MULTIPLE_TASK_ASSIGNMENTS_ERROR_IO,
      error_message);
  opstats_logger_->StopLoggingForTheCurrentPhase();
  LogMultipleTaskAssignmentsLatency(time_before_multiple_task_assignments,
                                    reference_time);
}

void PhaseLoggerImpl::LogMultipleTaskAssignmentsPayloadIOError(
    absl::Status error_status) {
  std::string error_message =
      GetErrorMessage(error_status, kMultipleTaskAssignmentsPrefix,
                      /* keep_error_message= */ true);
  event_publisher_->PublishMultipleTaskAssignmentsPayloadIOError(error_message);
  // Non-terminal events, we don't stop PhaseStats logging.
  opstats_logger_->AddEventWithErrorMessage(
      OperationalStats::Event::
          EVENT_KIND_MULTIPLE_TASK_ASSIGNMENTS_ERROR_PAYLOAD_IO,
      error_message);
}

void PhaseLoggerImpl::LogMultipleTaskAssignmentsInvalidPayload(
    absl::string_view error_message) {
  log_manager_->LogDiag(
      ProdDiagCode::BACKGROUND_TRAINING_FAILED_CANNOT_PARSE_PLAN);
  event_publisher_->PublishMultipleTaskAssignmentsInvalidPayload(error_message);
  // Non-terminal events, we don't stop PhaseStats logging.
  opstats_logger_->AddEventWithErrorMessage(
      OperationalStats::Event::
          EVENT_KIND_MULTIPLE_TASK_ASSIGNMENTS_ERROR_INVALID_PAYLOAD,
      std::string(error_message));
}

void PhaseLoggerImpl::LogMultipleTaskAssignmentsClientInterrupted(
    absl::Status error_status, const NetworkStats& network_stats,
    absl::Time time_before_multiple_task_assignments,
    absl::Time reference_time) {
  std::string error_message =
      GetErrorMessage(error_status, kMultipleTaskAssignmentsPrefix,
                      /* keep_error_message= */ true);
  event_publisher_->PublishMultipleTaskAssignmentsClientInterrupted(
      error_message, network_stats,
      absl::Now() - time_before_multiple_task_assignments);
  opstats_logger_->AddEventWithErrorMessage(
      OperationalStats::Event::
          EVENT_KIND_MULTIPLE_TASK_ASSIGNMENTS_CLIENT_INTERRUPTED,
      error_message);
  opstats_logger_->StopLoggingForTheCurrentPhase();
  LogMultipleTaskAssignmentsLatency(time_before_multiple_task_assignments,
                                    reference_time);
}

void PhaseLoggerImpl::LogMultipleTaskAssignmentsServerAborted(
    absl::Status error_status, const NetworkStats& network_stats,
    absl::Time time_before_multiple_task_assignments,
    absl::Time reference_time) {
  std::string error_message =
      GetErrorMessage(error_status, kMultipleTaskAssignmentsPrefix,
                      /* keep_error_message= */ true);
  event_publisher_->PublishMultipleTaskAssignmentsServerAborted(
      error_message, network_stats,
      absl::Now() - time_before_multiple_task_assignments);
  opstats_logger_->AddEventWithErrorMessage(
      OperationalStats::Event::
          EVENT_KIND_MULTIPLE_TASK_ASSIGNMENTS_SERVER_ABORTED,
      error_message);
  opstats_logger_->StopLoggingForTheCurrentPhase();
  LogMultipleTaskAssignmentsLatency(time_before_multiple_task_assignments,
                                    reference_time);
}

void PhaseLoggerImpl::LogMultipleTaskAssignmentsTurnedAway(
    const NetworkStats& network_stats,
    absl::Time time_before_multiple_task_assignments,
    absl::Time reference_time) {
  event_publisher_->PublishMultipleTaskAssignmentsTurnedAway(
      network_stats, absl::Now() - time_before_multiple_task_assignments);
  opstats_logger_->AddEvent(
      OperationalStats::Event::EVENT_KIND_MULTIPLE_TASK_ASSIGNMENTS_REJECTED);
  opstats_logger_->StopLoggingForTheCurrentPhase();
  LogMultipleTaskAssignmentsLatency(time_before_multiple_task_assignments,
                                    reference_time);
}

void PhaseLoggerImpl::LogMultipleTaskAssignmentsPlanUriReceived(
    const NetworkStats& network_stats,
    absl::Time time_before_multiple_task_assignments) {
  event_publisher_->PublishMultipleTaskAssignmentsPlanUriReceived(
      network_stats, absl::Now() - time_before_multiple_task_assignments);
  // Non-terminal events, we don't stop PhaseStats logging.
  opstats_logger_->AddEvent(
      OperationalStats::Event::
          EVENT_KIND_MULTIPLE_TASK_ASSIGNMENTS_PLAN_URI_RECEIVED);
}

void PhaseLoggerImpl::LogMultipleTaskAssignmentsPlanUriPartialReceived(
    const NetworkStats& network_stats,
    absl::Time time_before_multiple_task_assignments) {
  event_publisher_->PublishMultipleTaskAssignmentsPlanUriPartialReceived(
      network_stats, absl::Now() - time_before_multiple_task_assignments);
  // Non-terminal events, we don't stop PhaseStats logging.
  opstats_logger_->AddEvent(
      OperationalStats::Event::
          EVENT_KIND_MULTIPLE_TASK_ASSIGNMENTS_PLAN_URI_PARTIAL_RECEIVED);
}

void PhaseLoggerImpl::LogMultipleTaskAssignmentsPartialCompleted(
    const NetworkStats& network_stats,
    absl::Time time_before_multiple_task_assignments,
    absl::Time time_before_plan_download, absl::Time reference_time) {
  absl::Duration duration = absl::Now() - time_before_plan_download;
  event_publisher_->PublishMultipleTaskAssignmentsPartialCompleted(
      network_stats, duration);
  opstats_logger_->AddEvent(
      OperationalStats::Event::
          EVENT_KIND_MULTIPLE_TASK_ASSIGNMENTS_PARTIAL_COMPLETED);
  opstats_logger_->StopLoggingForTheCurrentPhase();
  LogMultipleTaskAssignmentsLatency(time_before_multiple_task_assignments,
                                    reference_time);
}

void PhaseLoggerImpl::LogMultipleTaskAssignmentsCompleted(
    const NetworkStats& network_stats,
    absl::Time time_before_multiple_task_assignments,
    absl::Time time_before_plan_download, absl::Time reference_time) {
  absl::Duration duration = absl::Now() - time_before_plan_download;
  event_publisher_->PublishMultipleTaskAssignmentsCompleted(network_stats,
                                                            duration);
  opstats_logger_->AddEvent(
      OperationalStats::Event::EVENT_KIND_MULTIPLE_TASK_ASSIGNMENTS_COMPLETED);
  opstats_logger_->StopLoggingForTheCurrentPhase();
  LogMultipleTaskAssignmentsLatency(time_before_multiple_task_assignments,
                                    reference_time);
}

void PhaseLoggerImpl::LogCheckinStarted() {
  // Log that we are about to check in with the server.
  event_publisher_->PublishCheckin();
  opstats_logger_->StartLoggingForPhase(OperationalStats::PhaseStats::CHECKIN);
  opstats_logger_->AddEvent(
      OperationalStats::Event::EVENT_KIND_CHECKIN_STARTED);
}

void PhaseLoggerImpl::LogCheckinIOError(absl::Status error_status,
                                        const NetworkStats& network_stats,
                                        absl::Time time_before_checkin,
                                        absl::Time reference_time) {
  std::string error_message = GetErrorMessage(error_status, kCheckinErrorPrefix,
                                              /* keep_error_message= */ true);
  event_publisher_->PublishCheckinIoError(error_message, network_stats,
                                          absl::Now() - time_before_checkin);
  opstats_logger_->AddEventWithErrorMessage(
      OperationalStats::Event::EVENT_KIND_CHECKIN_ERROR_IO, error_message);
  opstats_logger_->StopLoggingForTheCurrentPhase();
  LogCheckinLatency(time_before_checkin, reference_time);
}

void PhaseLoggerImpl::LogCheckinClientInterrupted(
    absl::Status error_status, const NetworkStats& network_stats,
    absl::Time time_before_checkin, absl::Time reference_time) {
  std::string error_message = GetErrorMessage(error_status, kCheckinErrorPrefix,
                                              /* keep_error_message= */ true);
  event_publisher_->PublishCheckinClientInterrupted(
      error_message, network_stats, absl::Now() - time_before_checkin);
  opstats_logger_->AddEventWithErrorMessage(
      OperationalStats::Event::EVENT_KIND_CHECKIN_CLIENT_INTERRUPTED,
      error_message);
  opstats_logger_->StopLoggingForTheCurrentPhase();
  LogCheckinLatency(time_before_checkin, reference_time);
}

void PhaseLoggerImpl::LogCheckinServerAborted(absl::Status error_status,
                                              const NetworkStats& network_stats,
                                              absl::Time time_before_checkin,
                                              absl::Time reference_time) {
  std::string error_message = GetErrorMessage(error_status, kCheckinErrorPrefix,
                                              /* keep_error_message= */ true);
  event_publisher_->PublishCheckinServerAborted(
      error_message, network_stats, absl::Now() - time_before_checkin);
  opstats_logger_->AddEventWithErrorMessage(
      OperationalStats::Event::EVENT_KIND_CHECKIN_SERVER_ABORTED,
      error_message);
  opstats_logger_->StopLoggingForTheCurrentPhase();
  LogCheckinLatency(time_before_checkin, reference_time);
}

void PhaseLoggerImpl::LogCheckinTurnedAway(const NetworkStats& network_stats,
                                           absl::Time time_before_checkin,
                                           absl::Time reference_time) {
  event_publisher_->PublishRejected(network_stats,
                                    absl::Now() - time_before_checkin);
  opstats_logger_->AddEvent(
      OperationalStats::Event::EVENT_KIND_CHECKIN_REJECTED);
  opstats_logger_->StopLoggingForTheCurrentPhase();
  LogCheckinLatency(time_before_checkin, reference_time);
}

void PhaseLoggerImpl::LogCheckinInvalidPayload(
    absl::string_view error_message, const NetworkStats& network_stats,
    absl::Time time_before_checkin, absl::Time reference_time) {
  log_manager_->LogDiag(
      ProdDiagCode::BACKGROUND_TRAINING_FAILED_CANNOT_PARSE_PLAN);
  event_publisher_->PublishCheckinInvalidPayload(
      error_message, network_stats, absl::Now() - time_before_checkin);
  opstats_logger_->AddEventWithErrorMessage(
      OperationalStats::Event::EVENT_KIND_CHECKIN_ERROR_INVALID_PAYLOAD,
      std::string(error_message));
  opstats_logger_->StopLoggingForTheCurrentPhase();
  LogCheckinLatency(time_before_checkin, reference_time);
}

void PhaseLoggerImpl::LogCheckinPlanUriReceived(
    absl::string_view task_name, const NetworkStats& network_stats,
    absl::Time time_before_checkin) {
  event_publisher_->PublishCheckinPlanUriReceived(
      network_stats, absl::Now() - time_before_checkin);
  // Non-terminal event, we don't stop PhaseStats logging.
  opstats_logger_->AddEventAndSetTaskName(
      std::string(task_name),
      OperationalStats::Event::EVENT_KIND_CHECKIN_PLAN_URI_RECEIVED);
}

void PhaseLoggerImpl::LogCheckinCompleted(absl::string_view task_name,
                                          const NetworkStats& network_stats,
                                          absl::Time time_before_checkin,
                                          absl::Time time_before_plan_download,
                                          absl::Time reference_time) {
  absl::Duration duration = absl::Now() - time_before_plan_download;
  event_publisher_->PublishCheckinFinishedV2(network_stats, duration);
  // We already have set the task name when LogCheckinPlanUriReceived was
  // called, so we only have to add the event.
  opstats_logger_->AddEvent(
      OperationalStats::Event::EVENT_KIND_CHECKIN_ACCEPTED);
  opstats_logger_->StopLoggingForTheCurrentPhase();
  // The 'EligibilityEvalCheckinLatency' should cover the whole period from
  // eligibility eval checkin to completion (and not just the period from EET
  // plan URIs being received to completion).
  LogCheckinLatency(time_before_checkin, reference_time);
}

void PhaseLoggerImpl::MaybeLogCollectionFirstAccessTime(
    absl::string_view collection_uri) {
  opstats_logger_->RecordCollectionFirstAccessTime(collection_uri, absl::Now());
}

void PhaseLoggerImpl::LogComputationStarted(absl::string_view task_name) {
  event_publisher_->PublishComputationStarted();
  opstats_logger_->StartLoggingForPhase(
      OperationalStats::PhaseStats::COMPUTATION);
  opstats_logger_->AddEventAndSetTaskName(
      std::string(task_name),
      OperationalStats::Event::EVENT_KIND_COMPUTATION_STARTED);
}

void PhaseLoggerImpl::LogComputationInvalidArgument(
    absl::Status error_status, const ExampleStats& example_stats,
    const NetworkStats& network_stats, absl::Time run_plan_start_time) {
  std::string error_message =
      GetErrorMessage(error_status, kComputationErrorPrefix,
                      /* keep_error_message= */ true);
  log_manager_->LogDiag(
      ProdDiagCode::BACKGROUND_TRAINING_FAILED_PLAN_FAILS_SANITY_CHECK);
  event_publisher_->PublishComputationInvalidArgument(
      error_message, example_stats, network_stats,
      absl::Now() - run_plan_start_time);
  opstats_logger_->AddEventWithErrorMessage(
      OperationalStats::Event::EVENT_KIND_COMPUTATION_ERROR_INVALID_ARGUMENT,
      error_message);
  opstats_logger_->StopLoggingForTheCurrentPhase();
}

void PhaseLoggerImpl::LogComputationIOError(absl::Status error_status,
                                            const ExampleStats& example_stats,
                                            const NetworkStats& network_stats,
                                            absl::Time run_plan_start_time) {
  std::string error_message =
      GetErrorMessage(error_status, kComputationErrorPrefix,
                      /* keep_error_message= */ true);
  event_publisher_->PublishComputationIOError(
      error_message, example_stats, network_stats,
      absl::Now() - run_plan_start_time);
  opstats_logger_->AddEventWithErrorMessage(
      OperationalStats::Event::EVENT_KIND_COMPUTATION_ERROR_IO, error_message);
  opstats_logger_->StopLoggingForTheCurrentPhase();
}

void PhaseLoggerImpl::LogComputationExampleIteratorError(
    absl::Status error_status, const ExampleStats& example_stats,
    const NetworkStats& network_stats, absl::Time run_plan_start_time) {
  std::string error_message = GetErrorMessage(
      error_status, kComputationErrorPrefix, /* keep_error_message= */ true);
  event_publisher_->PublishComputationExampleIteratorError(
      error_message, example_stats, network_stats,
      absl::Now() - run_plan_start_time);
  opstats_logger_->AddEventWithErrorMessage(
      OperationalStats::Event::EVENT_KIND_COMPUTATION_ERROR_EXAMPLE_ITERATOR,
      error_message);
  opstats_logger_->StopLoggingForTheCurrentPhase();
}

void PhaseLoggerImpl::LogComputationTensorflowError(
    absl::Status error_status, const ExampleStats& example_stats,
    const NetworkStats& network_stats, absl::Time run_plan_start_time,
    absl::Time reference_time) {
  std::string error_message = GetErrorMessage(
      error_status, kComputationErrorPrefix, log_tensorflow_error_messages_);
  event_publisher_->PublishComputationTensorflowError(
      error_message, example_stats, network_stats,
      absl::Now() - run_plan_start_time);
  opstats_logger_->AddEventWithErrorMessage(
      OperationalStats::Event::EVENT_KIND_COMPUTATION_ERROR_TENSORFLOW,
      error_message);
  opstats_logger_->StopLoggingForTheCurrentPhase();
  LogComputationLatency(run_plan_start_time, reference_time);
}

void PhaseLoggerImpl::LogComputationInterrupted(
    absl::Status error_status, const ExampleStats& example_stats,
    const NetworkStats& network_stats, absl::Time run_plan_start_time,
    absl::Time reference_time) {
  std::string error_message =
      GetErrorMessage(error_status, kComputationErrorPrefix,
                      /* keep_error_message= */ true);
  event_publisher_->PublishComputationInterrupted(
      error_message, example_stats, network_stats,
      absl::Now() - run_plan_start_time);
  opstats_logger_->AddEventWithErrorMessage(
      OperationalStats::Event::EVENT_KIND_COMPUTATION_CLIENT_INTERRUPTED,
      error_message);
  opstats_logger_->StopLoggingForTheCurrentPhase();
  LogComputationLatency(run_plan_start_time, reference_time);
}

void PhaseLoggerImpl::LogComputationCompleted(
    const ExampleStats& example_stats, const NetworkStats& network_stats,
    absl::Time run_plan_start_time, absl::Time reference_time,
    std::optional<int64_t> min_sep_policy_index) {
  event_publisher_->PublishComputationCompleted(
      example_stats, network_stats, absl::Now() - run_plan_start_time);
  opstats_logger_->AddEvent(
      OperationalStats::Event::EVENT_KIND_COMPUTATION_FINISHED);
  if (min_sep_policy_index.has_value()) {
    opstats_logger_->SetMinSepPolicyIndex(min_sep_policy_index.value());
  }
  opstats_logger_->StopLoggingForTheCurrentPhase();
  log_manager_->LogToLongHistogram(
      HistogramCounters::TRAINING_OVERALL_EXAMPLE_SIZE,
      example_stats.example_size_bytes);
  log_manager_->LogToLongHistogram(
      HistogramCounters::TRAINING_OVERALL_EXAMPLE_COUNT,
      example_stats.example_count);
  LogComputationLatency(run_plan_start_time, reference_time);
}

absl::Status PhaseLoggerImpl::LogResultUploadStarted() {
  opstats_logger_->StartLoggingForPhase(OperationalStats::PhaseStats::UPLOAD);
  opstats_logger_->AddEvent(
      OperationalStats::Event::EVENT_KIND_RESULT_UPLOAD_STARTED);
  // Commit the run data accumulated thus far to Opstats and fail if
  // something goes wrong.
  FCP_RETURN_IF_ERROR(opstats_logger_->CommitToStorage());
  event_publisher_->PublishResultUploadStarted();
  return absl::OkStatus();
}

void PhaseLoggerImpl::LogResultUploadIOError(
    absl::Status error_status, const NetworkStats& network_stats,
    absl::Time time_before_result_upload, absl::Time reference_time) {
  std::string error_message =
      GetErrorMessage(error_status, kResultUploadErrorPrefix,
                      /* keep_error_message= */ true);
  event_publisher_->PublishResultUploadIOError(
      error_message, network_stats, absl::Now() - time_before_result_upload);
  opstats_logger_->AddEventWithErrorMessage(
      OperationalStats::Event::EVENT_KIND_RESULT_UPLOAD_ERROR_IO,
      error_message);
  opstats_logger_->StopLoggingForTheCurrentPhase();
  LogReportLatency(time_before_result_upload, reference_time);
}

void PhaseLoggerImpl::LogResultUploadClientInterrupted(
    absl::Status error_status, const NetworkStats& network_stats,
    absl::Time time_before_result_upload, absl::Time reference_time) {
  std::string error_message =
      GetErrorMessage(error_status, kResultUploadErrorPrefix,
                      /* keep_error_message= */ true);
  event_publisher_->PublishResultUploadClientInterrupted(
      error_message, network_stats, absl::Now() - time_before_result_upload);
  opstats_logger_->AddEventWithErrorMessage(
      OperationalStats::Event::EVENT_KIND_RESULT_UPLOAD_CLIENT_INTERRUPTED,
      error_message);
  opstats_logger_->StopLoggingForTheCurrentPhase();
  LogReportLatency(time_before_result_upload, reference_time);
}

void PhaseLoggerImpl::LogResultUploadServerAborted(
    absl::Status error_status, const NetworkStats& network_stats,
    absl::Time time_before_result_upload, absl::Time reference_time) {
  std::string error_message =
      GetErrorMessage(error_status, kResultUploadErrorPrefix,
                      /* keep_error_message= */ true);
  event_publisher_->PublishResultUploadServerAborted(
      error_message, network_stats, absl::Now() - time_before_result_upload);
  opstats_logger_->AddEventWithErrorMessage(
      OperationalStats::Event::EVENT_KIND_RESULT_UPLOAD_SERVER_ABORTED,
      error_message);
  opstats_logger_->StopLoggingForTheCurrentPhase();
  LogReportLatency(time_before_result_upload, reference_time);
}

void PhaseLoggerImpl::LogResultUploadCompleted(
    const NetworkStats& network_stats, absl::Time time_before_result_upload,
    absl::Time reference_time) {
  event_publisher_->PublishResultUploadCompleted(
      network_stats, absl::Now() - time_before_result_upload);
  opstats_logger_->AddEvent(
      OperationalStats::Event::EVENT_KIND_RESULT_UPLOAD_FINISHED);
  opstats_logger_->StopLoggingForTheCurrentPhase();
  LogReportLatency(time_before_result_upload, reference_time);
}

absl::Status PhaseLoggerImpl::LogFailureUploadStarted() {
  opstats_logger_->StartLoggingForPhase(OperationalStats::PhaseStats::UPLOAD);
  opstats_logger_->AddEvent(
      OperationalStats::Event::EVENT_KIND_FAILURE_UPLOAD_STARTED);
  // Commit the run data accumulated thus far to Opstats and fail if
  // something goes wrong.
  FCP_RETURN_IF_ERROR(opstats_logger_->CommitToStorage());
  event_publisher_->PublishFailureUploadStarted();
  return absl::OkStatus();
}

void PhaseLoggerImpl::LogFailureUploadIOError(
    absl::Status error_status, const NetworkStats& network_stats,
    absl::Time time_before_failure_upload, absl::Time reference_time) {
  std::string error_message =
      GetErrorMessage(error_status, kFailureUploadErrorPrefix,
                      /* keep_error_message= */ true);
  event_publisher_->PublishFailureUploadIOError(
      error_message, network_stats, absl::Now() - time_before_failure_upload);
  opstats_logger_->AddEventWithErrorMessage(
      OperationalStats::Event::EVENT_KIND_FAILURE_UPLOAD_ERROR_IO,
      error_message);
  opstats_logger_->StopLoggingForTheCurrentPhase();
  LogReportLatency(time_before_failure_upload, reference_time);
}

void PhaseLoggerImpl::LogFailureUploadClientInterrupted(
    absl::Status error_status, const NetworkStats& network_stats,
    absl::Time time_before_failure_upload, absl::Time reference_time) {
  std::string error_message =
      GetErrorMessage(error_status, kFailureUploadErrorPrefix,
                      /* keep_error_message= */ true);
  event_publisher_->PublishFailureUploadClientInterrupted(
      error_message, network_stats, absl::Now() - time_before_failure_upload);
  opstats_logger_->AddEventWithErrorMessage(
      OperationalStats::Event::EVENT_KIND_FAILURE_UPLOAD_CLIENT_INTERRUPTED,
      error_message);
  opstats_logger_->StopLoggingForTheCurrentPhase();
  LogReportLatency(time_before_failure_upload, reference_time);
}

void PhaseLoggerImpl::LogFailureUploadServerAborted(
    absl::Status error_status, const NetworkStats& network_stats,
    absl::Time time_before_failure_upload, absl::Time reference_time) {
  std::string error_message =
      GetErrorMessage(error_status, kFailureUploadErrorPrefix,
                      /* keep_error_message= */ true);
  event_publisher_->PublishFailureUploadServerAborted(
      error_message, network_stats, absl::Now() - time_before_failure_upload);
  opstats_logger_->AddEventWithErrorMessage(
      OperationalStats::Event::EVENT_KIND_FAILURE_UPLOAD_SERVER_ABORTED,
      error_message);
  opstats_logger_->StopLoggingForTheCurrentPhase();
  LogReportLatency(time_before_failure_upload, reference_time);
}

void PhaseLoggerImpl::LogFailureUploadCompleted(
    const NetworkStats& network_stats, absl::Time time_before_failure_upload,
    absl::Time reference_time) {
  event_publisher_->PublishFailureUploadCompleted(
      network_stats, absl::Now() - time_before_failure_upload);
  opstats_logger_->AddEvent(
      OperationalStats::Event::EVENT_KIND_FAILURE_UPLOAD_FINISHED);
  opstats_logger_->StopLoggingForTheCurrentPhase();
  LogReportLatency(time_before_failure_upload, reference_time);
}

void PhaseLoggerImpl::LogTimeSince(HistogramCounters histogram_counter,
                                   absl::Time reference_time) {
  absl::Duration duration = absl::Now() - reference_time;
  log_manager_->LogToLongHistogram(histogram_counter,
                                   absl::ToInt64Milliseconds(duration));
}

void PhaseLoggerImpl::LogEligibilityEvalCheckinLatency(
    absl::Time time_before_checkin) {
  LogTimeSince(HistogramCounters::TRAINING_FL_ELIGIBILITY_EVAL_CHECKIN_LATENCY,
               time_before_checkin);
}

void PhaseLoggerImpl::LogEligibilityEvalComputationLatency(
    absl::Time run_plan_start_time, absl::Time reference_time) {
  LogTimeSince(HistogramCounters::TRAINING_RUN_PHASE_LATENCY,
               run_plan_start_time);
  LogTimeSince(HistogramCounters::TRAINING_RUN_PHASE_END_TIME, reference_time);
}

void PhaseLoggerImpl::LogMultipleTaskAssignmentsLatency(
    absl::Time time_before_multiple_task_assignments,
    absl::Time reference_time) {
  LogTimeSince(HistogramCounters::TRAINING_FL_MULTIPLE_TASK_ASSIGNMENTS_LATENCY,
               time_before_multiple_task_assignments);
  LogTimeSince(
      HistogramCounters::TRAINING_FL_MULTIPLE_TASK_ASSIGNMENTS_END_TIME,
      reference_time);
}

void PhaseLoggerImpl::LogCheckinLatency(absl::Time time_before_checkin,
                                        absl::Time reference_time) {
  LogTimeSince(HistogramCounters::TRAINING_FL_CHECKIN_LATENCY,
               time_before_checkin);
  LogTimeSince(HistogramCounters::TRAINING_FL_CHECKIN_END_TIME, reference_time);
}

void PhaseLoggerImpl::LogComputationLatency(absl::Time run_plan_start_time,
                                            absl::Time reference_time) {
  LogTimeSince(HistogramCounters::TRAINING_RUN_PHASE_LATENCY,
               run_plan_start_time);
  LogTimeSince(HistogramCounters::TRAINING_RUN_PHASE_END_TIME, reference_time);
}

void PhaseLoggerImpl::LogReportLatency(absl::Time time_before_report,
                                       absl::Time reference_time) {
  LogTimeSince(HistogramCounters::TRAINING_FL_REPORT_RESULTS_LATENCY,
               time_before_report);
  LogTimeSince(HistogramCounters::TRAINING_FL_REPORT_RESULTS_END_TIME,
               reference_time);
}

std::string PhaseLoggerImpl::GetErrorMessage(absl::Status error_status,
                                             absl::string_view error_prefix,
                                             bool keep_error_message) {
  return absl::StrCat(error_prefix, "code: ", error_status.code(), ", error: ",
                      keep_error_message ? error_status.message() : "");
}

}  // namespace client
}  // namespace fcp
