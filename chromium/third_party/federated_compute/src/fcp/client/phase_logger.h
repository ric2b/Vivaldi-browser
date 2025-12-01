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
#ifndef FCP_CLIENT_PHASE_LOGGER_H_
#define FCP_CLIENT_PHASE_LOGGER_H_

#include <cstdint>
#include <optional>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "fcp/client/stats.h"
#include "fcp/protos/federated_api.pb.h"

namespace fcp {
namespace client {

class PhaseLogger {
 public:
  virtual ~PhaseLogger() = default;
  virtual void UpdateRetryWindowAndNetworkStats(
      const ::google::internal::federatedml::v2::RetryWindow& retry_window,
      const NetworkStats& network_stats) = 0;
  virtual void SetModelIdentifier(absl::string_view model_identifier) = 0;

  // Called when a run was started but immediately aborted.
  virtual void LogTaskNotStarted(absl::string_view error_message) = 0;
  // Called when a run was started but the runtime failed to initialize a
  // noncritical component, and execution continue.
  virtual void LogNonfatalInitializationError(absl::Status error_status) = 0;
  // Called when a run was started but the runtime failed to initialize a
  // component, and execution was halted.
  virtual void LogFatalInitializationError(absl::Status error_status) = 0;

  // Eligibility eval check-in phase.
  // Called when an eligibility eval check-in starts.
  virtual void LogEligibilityEvalCheckinStarted() = 0;
  // Called when an IO error is encountered during eligibility eval check-in.
  virtual void LogEligibilityEvalCheckinIOError(
      absl::Status error_status, const NetworkStats& network_stats,
      absl::Time time_before_checkin) = 0;
  // Called when an invalid payload is received from the eligibility eval
  // check-in result.
  virtual void LogEligibilityEvalCheckinInvalidPayloadError(
      absl::string_view error_message, const NetworkStats& network_stats,
      absl::Time time_before_checkin) = 0;
  // Called when the eligibility eval check-in is interrupted by the client.
  virtual void LogEligibilityEvalCheckinClientInterrupted(
      absl::Status error_status, const NetworkStats& network_stats,
      absl::Time time_before_checkin) = 0;
  // Called when the eligibility eval check-in is aborted by the server.
  virtual void LogEligibilityEvalCheckinServerAborted(
      absl::Status error_status, const NetworkStats& network_stats,
      absl::Time time_before_checkin) = 0;
  // Called when eligibility eval is not configured.
  virtual void LogEligibilityEvalNotConfigured(
      const NetworkStats& network_stats, absl::Time time_before_checkin) = 0;
  // Called when eligibility eval check-in request is turned away by the server.
  virtual void LogEligibilityEvalCheckinTurnedAway(
      const NetworkStats& network_stats, absl::Time time_before_checkin) = 0;
  virtual void LogEligibilityEvalCheckinPlanUriReceived(
      const NetworkStats& network_stats, absl::Time time_before_checkin) = 0;
  // Called when a valid eligibility eval plan is received.
  virtual void LogEligibilityEvalCheckinCompleted(
      const NetworkStats& network_stats, absl::Time time_before_checkin,
      absl::Time time_before_plan_download) = 0;

  // Eligibility eval computation phase.
  // Called when the eligibility eval computation starts.
  virtual void LogEligibilityEvalComputationStarted() = 0;
  // Called when the input parameters for the eligibility eval task are invalid.
  virtual void LogEligibilityEvalComputationInvalidArgument(
      absl::Status error_status, const ExampleStats& example_stats,
      absl::Time run_plan_start_time) = 0;
  // Called when an example store error happened during eligibility eval
  // computation.
  virtual void LogEligibilityEvalComputationExampleIteratorError(
      absl::Status error_status, const ExampleStats& example_stats,
      absl::Time run_plan_start_time) = 0;
  // Called when a tensorflow error happened during eligibility eval
  // computation.
  virtual void LogEligibilityEvalComputationTensorflowError(
      absl::Status error_status, const ExampleStats& example_stats,
      absl::Time run_plan_start_time, absl::Time reference_time) = 0;
  // Called when an internal error happened during eligibility eval computation.
  virtual void LogEligibilityEvalComputationIOError(
      absl::Status error_status, const ExampleStats& example_stats,
      absl::Time run_plan_start_time, absl::Time reference_time) = 0;
  // Called when the eligibility eval computation is interrupted.
  virtual void LogEligibilityEvalComputationInterrupted(
      absl::Status error_status, const ExampleStats& example_stats,
      absl::Time run_plan_start_time, absl::Time reference_time) = 0;
  // Called when a native eligibility policy computation produces an error but
  // client execution is allowed to continue.
  virtual void LogEligibilityEvalComputationErrorNonfatal(
      absl::Status error_status) = 0;
  // Called when the eligibility eval computation is completed.
  virtual void LogEligibilityEvalComputationCompleted(
      const ExampleStats& example_stats, absl::Time run_plan_start_time,
      absl::Time reference_time) = 0;

  // Multiple task assignments phase.
  // Called when a PerformMultipleTaskAssignments starts.
  virtual void LogMultipleTaskAssignmentsStarted() = 0;
  // Called when an IO error occurred during multiple task assignments.
  virtual void LogMultipleTaskAssignmentsIOError(
      absl::Status error_status, const NetworkStats& network_stats,
      absl::Time time_before_multiple_task_assignments,
      absl::Time reference_time) = 0;
  // Called when an IO error occurred during the payload retrieval phase of
  // multiple task assignments. This is not a terminating event, it could be
  // called multiple times during one call of PerformMultipleTaskAssignments,
  // and the client may continue running other tasks for which it did manage to
  // retrieve the payloads after this event is logged.
  virtual void LogMultipleTaskAssignmentsPayloadIOError(
      absl::Status error_status) = 0;
  // Called when an invalid payload is received from the multiple task
  // assignments result.
  // This is not a terminating event, it could be called multiple times during
  // one call of PerformMultipleTaskAssignments, and the client may continue
  // running other tasks for which it did manage to retrieve the payloads after
  // this event is logged.
  virtual void LogMultipleTaskAssignmentsInvalidPayload(
      absl::string_view error_message) = 0;
  // Called when multiple task assignments is interrupted by the client.
  virtual void LogMultipleTaskAssignmentsClientInterrupted(
      absl::Status error_status, const NetworkStats& network_stats,
      absl::Time time_before_multiple_task_assignments,
      absl::Time reference_time) = 0;
  // Called when multiple task assignments is aborted by the server.
  virtual void LogMultipleTaskAssignmentsServerAborted(
      absl::Status error_status, const NetworkStats& network_stats,
      absl::Time time_before_multiple_task_assignments,
      absl::Time reference_time) = 0;
  // Called when the client issues multiple task assignments, but the server
  // assigned zero task to the client.
  virtual void LogMultipleTaskAssignmentsTurnedAway(
      const NetworkStats& network_stats,
      absl::Time time_before_multiple_task_assignments,
      absl::Time reference_time) = 0;
  // Called when the plan uris for all the requested tasks are received during
  // multiple task assignments.
  virtual void LogMultipleTaskAssignmentsPlanUriReceived(
      const NetworkStats& network_stats,
      absl::Time time_before_multiple_task_assignments) = 0;
  // Called when the plan uris for some of the requested tasks are received
  // during multiple task assignments.
  virtual void LogMultipleTaskAssignmentsPlanUriPartialReceived(
      const NetworkStats& network_stats,
      absl::Time time_before_multiple_task_assignments) = 0;
  // Called when check-in is completed, but one or more invalid payload or IO
  // errors have occurred.
  virtual void LogMultipleTaskAssignmentsPartialCompleted(
      const NetworkStats& network_stats,
      absl::Time time_before_multiple_task_assignments,
      absl::Time time_before_plan_download, absl::Time reference_time) = 0;
  // Called when multiple task assignments is completed successfully.
  virtual void LogMultipleTaskAssignmentsCompleted(
      const NetworkStats& network_stats,
      absl::Time time_before_multiple_task_assignments,
      absl::Time time_before_plan_download, absl::Time reference_time) = 0;

  // Check-in phase.
  // Called when a regular check-in starts.
  virtual void LogCheckinStarted() = 0;
  // Called when an IO error occurred during check-in.
  virtual void LogCheckinIOError(absl::Status error_status,
                                 const NetworkStats& network_stats,
                                 absl::Time time_before_checkin,
                                 absl::Time reference_time) = 0;
  // Called when an invalid payload is received from the check-in result.
  virtual void LogCheckinInvalidPayload(absl::string_view error_message,
                                        const NetworkStats& network_stats,
                                        absl::Time time_before_checkin,
                                        absl::Time reference_time) = 0;
  // Called when check-in is interrupted by the client.
  virtual void LogCheckinClientInterrupted(absl::Status error_status,
                                           const NetworkStats& network_stats,
                                           absl::Time time_before_checkin,
                                           absl::Time reference_time) = 0;
  // Called when check-in is aborted by the server.
  virtual void LogCheckinServerAborted(absl::Status error_status,
                                       const NetworkStats& network_stats,
                                       absl::Time time_before_checkin,
                                       absl::Time reference_time) = 0;
  // Called when the client's check-in request is turned away by the server.
  virtual void LogCheckinTurnedAway(const NetworkStats& network_stats,
                                    absl::Time time_before_checkin,
                                    absl::Time reference_time) = 0;
  virtual void LogCheckinPlanUriReceived(absl::string_view task_name,
                                         const NetworkStats& network_stats,
                                         absl::Time time_before_checkin) = 0;
  // Called when check-in is completed.
  virtual void LogCheckinCompleted(absl::string_view task_name,
                                   const NetworkStats& network_stats,
                                   absl::Time time_before_checkin,
                                   absl::Time time_before_plan_download,
                                   absl::Time reference_time) = 0;

  // Computation phase.
  // Called when collection is first accessed.
  virtual void MaybeLogCollectionFirstAccessTime(
      absl::string_view collection_uri) = 0;
  // Called when computation started.
  virtual void LogComputationStarted(absl::string_view task_name) = 0;
  // Called when the input parameters are invalid.
  virtual void LogComputationInvalidArgument(
      absl::Status error_status, const ExampleStats& example_stats,
      const NetworkStats& network_stats, absl::Time run_plan_start_time) = 0;
  // Called when an example store error occurred during computation.
  virtual void LogComputationExampleIteratorError(
      absl::Status error_status, const ExampleStats& example_stats,
      const NetworkStats& network_stats, absl::Time run_plan_start_time) = 0;
  // Called when an IO error happened during computation
  virtual void LogComputationIOError(absl::Status error_status,
                                     const ExampleStats& example_stats,
                                     const NetworkStats& network_stats,
                                     absl::Time run_plan_start_time) = 0;
  // Called when a tensorflow error happened during computation.
  virtual void LogComputationTensorflowError(absl::Status error_status,
                                             const ExampleStats& example_stats,
                                             const NetworkStats& network_stats,
                                             absl::Time run_plan_start_time,
                                             absl::Time reference_time) = 0;
  // Called when computation is interrupted.
  virtual void LogComputationInterrupted(absl::Status error_status,
                                         const ExampleStats& example_stats,
                                         const NetworkStats& network_stats,
                                         absl::Time run_plan_start_time,
                                         absl::Time reference_time) = 0;
  // Called when computation is completed.
  virtual void LogComputationCompleted(
      const ExampleStats& example_stats, const NetworkStats& network_stats,
      absl::Time run_plan_start_time, absl::Time reference_time,
      // The current index of MinimumSeparationPolicy that is applied to this
      // computation execution.
      std::optional<int64_t> min_sep_policy_index) = 0;

  // Result upload phase. Result upload only happens when all the previous
  // phases succeed.
  // Called when result upload started.
  virtual absl::Status LogResultUploadStarted() = 0;
  // Called when an IO error occurred during result upload.
  virtual void LogResultUploadIOError(absl::Status error_status,
                                      const NetworkStats& network_stats,
                                      absl::Time time_before_result_upload,
                                      absl::Time reference_time) = 0;
  // Called when the result upload is interrupted by the client.
  virtual void LogResultUploadClientInterrupted(
      absl::Status error_status, const NetworkStats& network_stats,
      absl::Time time_before_result_upload, absl::Time reference_time) = 0;
  // Called when the result upload is aborted by the server.
  virtual void LogResultUploadServerAborted(
      absl::Status error_status, const NetworkStats& network_stats,
      absl::Time time_before_result_upload, absl::Time reference_time) = 0;
  // Called when result upload is completed.
  virtual void LogResultUploadCompleted(const NetworkStats& network_stats,
                                        absl::Time time_before_result_upload,
                                        absl::Time reference_time) = 0;

  // Failure upload phase. Failure upload only happens when any of the previous
  // phases failed.
  // Called when failure upload starts.
  virtual absl::Status LogFailureUploadStarted() = 0;
  // Called when an IO error occurred during failure upload.
  virtual void LogFailureUploadIOError(absl::Status error_status,
                                       const NetworkStats& network_stats,
                                       absl::Time time_before_failure_upload,
                                       absl::Time reference_time) = 0;
  // Called when the failure upload is interrupted by the client.
  virtual void LogFailureUploadClientInterrupted(
      absl::Status error_status, const NetworkStats& network_stats,
      absl::Time time_before_failure_upload, absl::Time reference_time) = 0;
  // Called when the failure upload is aborted by the server.
  virtual void LogFailureUploadServerAborted(
      absl::Status error_status, const NetworkStats& network_stats,
      absl::Time time_before_failure_upload, absl::Time reference_time) = 0;
  // Called when the failure upload is completed.
  virtual void LogFailureUploadCompleted(const NetworkStats& network_stats,
                                         absl::Time time_before_failure_upload,
                                         absl::Time reference_time) = 0;
};

}  // namespace client
}  // namespace fcp

#endif  // FCP_CLIENT_PHASE_LOGGER_H_
