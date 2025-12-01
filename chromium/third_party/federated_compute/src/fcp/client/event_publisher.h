/*
 * Copyright 2019 Google LLC
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
#ifndef FCP_CLIENT_EVENT_PUBLISHER_H_
#define FCP_CLIENT_EVENT_PUBLISHER_H_

#include <string>

#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "fcp/client/stats.h"

namespace fcp {
namespace client {

class SecAggEventPublisher;

// An interface for publishing events that occur during training. This is a
// separate interface from LogManager because the reported events will typically
// be both reported to a cloud monitoring backend and to the Federated server as
// part of publishing results.
// All methods in here either succeed with OK, or fail with INVALID_ARGUMENT.
class EventPublisher {
 public:
  virtual ~EventPublisher() = default;

  // Publishes that the device is about to issue an eligibility eval check in
  // with the server.
  virtual void PublishEligibilityEvalCheckin() = 0;

  // Publishes that the device has finished its eligibility eval checkin with
  // the server, and received the URIs to download the eligibility eval plan
  // with, but hasn't actually downloaded them yet, along with information
  // how much data was transferred up to this point and how long that took.
  virtual void PublishEligibilityEvalPlanUriReceived(
      const NetworkStats& network_stats, absl::Duration phase_duration) = 0;

  // Publishes that the device has finished its eligibility eval checkin with
  // the server, and received an eligibility eval plan, along with information
  // how much data was transferred and how long that took.
  virtual void PublishEligibilityEvalPlanReceived(
      const NetworkStats& network_stats, absl::Duration phase_duration) = 0;

  // Publishes that the server did not return an eligibility eval task to the
  // client, along with information how much data was transferred and how long
  // that took.
  virtual void PublishEligibilityEvalNotConfigured(
      const NetworkStats& network_stats, absl::Duration phase_duration) = 0;

  // Publishes that the server rejected the device's eligibility eval checkin,
  // along with information how much data was downloaded and how long that took.
  virtual void PublishEligibilityEvalRejected(
      const NetworkStats& network_stats, absl::Duration phase_duration) = 0;

  // Publishes that the device is about to check in with the server.
  virtual void PublishCheckin() = 0;

  // Publishes that the device has finished checking in with the server, along
  // with information how much data was downloaded and how long that took.
  virtual void PublishCheckinFinished(const NetworkStats& network_stats,
                                      absl::Duration phase_duration) = 0;

  // Publishes that the server rejected the device.
  virtual void PublishRejected() = 0;

  // Publishes a TensorFlow error that happened in the given ClientExecution.
  virtual void PublishTensorFlowError(int example_count,
                                      absl::string_view error_message) = 0;

  // Publishes an I/O error (e.g. disk, network) that happened in the given
  // ClientExecution.
  virtual void PublishIoError(absl::string_view error_message) = 0;

  // Publishes an ExampleSelector error from the given ClientExecution.
  virtual void PublishExampleSelectorError(int example_count,
                                           absl::string_view error_message) = 0;

  // Publishes an interruption event for the given client execution.
  virtual void PublishInterruption(const ExampleStats& example_stats,
                                   absl::Time start_time) = 0;

  // Publishes that the task didn't start.
  virtual void PublishTaskNotStarted(absl::string_view error_message) = 0;

  // Publishes that the federated compute runtime failed to initialize a
  // noncritical component, but execution continued.
  virtual void PublishNonfatalInitializationError(
      absl::string_view error_message) = 0;
  // Publishes that the federated compute runtime failed to initialize a
  // component, and execution was halted.
  virtual void PublishFatalInitializationError(
      absl::string_view error_message) = 0;

  // Publish that an IO error was encountered during eligibility eval check-in.
  virtual void PublishEligibilityEvalCheckinIoError(
      absl::string_view error_message, const NetworkStats& network_stats,
      absl::Duration phase_duration) = 0;
  // Publish that the eligibility eval check-in is interrupted by the client.
  virtual void PublishEligibilityEvalCheckinClientInterrupted(
      absl::string_view error_message, const NetworkStats& network_stats,
      absl::Duration phase_duration) = 0;
  // Publish that the eligibility eval check-in is aborted by the server.
  virtual void PublishEligibilityEvalCheckinServerAborted(
      absl::string_view error_message, const NetworkStats& network_stats,
      absl::Duration phase_duration) = 0;
  // Publish that the eligibility eval check-in returned an invalid payload.
  virtual void PublishEligibilityEvalCheckinErrorInvalidPayload(
      absl::string_view error_message, const NetworkStats& network_stats,
      absl::Duration phase_duration) = 0;
  // Publish an eligibility eval task starts computation.
  virtual void PublishEligibilityEvalComputationStarted() = 0;
  // Publish that the eligibility eval task is invalid.
  virtual void PublishEligibilityEvalComputationInvalidArgument(
      absl::string_view error_message, const ExampleStats& example_stats,
      absl::Duration phase_duration) = 0;
  virtual void PublishEligibilityEvalComputationIOError(
      absl::string_view error_message,
      const ::fcp::client::ExampleStats& example_stats,
      absl::Duration phase_duration) = 0;
  // Publish an example iterator error occurred during eligibility eval task.
  virtual void PublishEligibilityEvalComputationExampleIteratorError(
      absl::string_view error_message, const ExampleStats& example_stats,
      absl::Duration phase_duration) = 0;
  // Publish that a tensorflow error occurred during eligibility eval task.
  virtual void PublishEligibilityEvalComputationTensorflowError(
      absl::string_view error_message, const ExampleStats& example_stats,
      absl::Duration phase_duration) = 0;
  // Publish that the client has interrupted the eligibility eval task.
  virtual void PublishEligibilityEvalComputationInterrupted(
      absl::string_view error_message, const ExampleStats& example_stats,
      absl::Duration phase_duration) = 0;
  // Publish that a native eligibility policy computation produced an error but
  // client execution was allowed to continue.
  virtual void PublishEligibilityEvalComputationErrorNonfatal(
      absl::string_view error_message) = 0;
  // Publish an eligibility eval task finished.
  virtual void PublishEligibilityEvalComputationCompleted(
      const ExampleStats& example_stats, absl::Duration phase_duration) = 0;

  // Publish that the client is about to start multiple task assignments.
  virtual void PublishMultipleTaskAssignmentsStarted() = 0;

  // Publish an IO error occurred during multiple task assignments.
  virtual void PublishMultipleTaskAssignmentsIOError(
      absl::string_view error_message, const NetworkStats& network_stats,
      absl::Duration phase_duration) = 0;

  // Publish an IO error occurred during the payload retrieval phase of multiple
  // task assignments.
  virtual void PublishMultipleTaskAssignmentsPayloadIOError(
      absl::string_view error_message) = 0;

  // Publish an invalid payload was downloaded from the multiple task
  // assignments.
  virtual void PublishMultipleTaskAssignmentsInvalidPayload(
      absl::string_view error_message) = 0;

  // Publish that the client interrupted the multiple task assignments.
  virtual void PublishMultipleTaskAssignmentsClientInterrupted(
      absl::string_view error_message, const NetworkStats& network_stats,
      absl::Duration phase_duration) = 0;

  // Publish that the server aborted the multiple task assignments.
  virtual void PublishMultipleTaskAssignmentsServerAborted(
      absl::string_view error_message, const NetworkStats& network_stats,
      absl::Duration phase_duration) = 0;

  // Publish that the server assigned the client zero task during multiple task
  // assignments.
  virtual void PublishMultipleTaskAssignmentsTurnedAway(
      const NetworkStats& network_stats, absl::Duration phase_duration) = 0;

  // Publishes that the device has finished multiple task assignments with the
  // server and received URIs for all of the requested tasks to download the
  // plan and checkpoint with, but hasn't yet downloaded those, along with
  // information how much data was transferred up to this point and how long
  // that took.
  virtual void PublishMultipleTaskAssignmentsPlanUriReceived(
      const NetworkStats& network_stats, absl::Duration phase_duration) = 0;

  // Publishes that the device has finished multiple task assignments with the
  // server and received URIs for some of the requested tasks to download the
  // plan and checkpoint with, but hasn't yet downloaded those, along with
  // information how much data was transferred up to this point and how long
  // that took.
  virtual void PublishMultipleTaskAssignmentsPlanUriPartialReceived(
      const NetworkStats& network_stats, absl::Duration phase_duration) = 0;

  // Publishes that the device has finished multiple task assignments with the
  // server, along with information how much data was transferred and how long
  // that took.  There was at least one failure happened when downloading plans
  // and checkpoints for the tasks.
  virtual void PublishMultipleTaskAssignmentsPartialCompleted(
      const NetworkStats& network_stats, absl::Duration phase_duration) = 0;

  // Publishes that the device has finished multiple task assignments with the
  // server, along with information how much data was transferred and how long
  // that took.
  virtual void PublishMultipleTaskAssignmentsCompleted(
      const NetworkStats& network_stats, absl::Duration phase_duration) = 0;

  // Publish an IO error occurred during regular check-in.
  virtual void PublishCheckinIoError(absl::string_view error_message,
                                     const NetworkStats& network_stats,
                                     absl::Duration phase_duration) = 0;
  // Publish that the client interrupted the regular check-in.
  virtual void PublishCheckinClientInterrupted(
      absl::string_view error_message, const NetworkStats& network_stats,
      absl::Duration phase_duration) = 0;
  // Publish that the server aborted the regular check-in.
  virtual void PublishCheckinServerAborted(absl::string_view error_message,
                                           const NetworkStats& network_stats,
                                           absl::Duration phase_duration) = 0;
  // Publish that an invalid payload was downloaded from the regular check-in.
  virtual void PublishCheckinInvalidPayload(absl::string_view error_message,
                                            const NetworkStats& network_stats,
                                            absl::Duration phase_duration) = 0;
  // Publishes that the server rejected the device, also logs network stats and
  // duration.
  virtual void PublishRejected(const NetworkStats& network_stats,
                               absl::Duration phase_duration) = 0;

  // Publishes that the device has finished checking in with the server and
  // received URIs to download the plan and checkpoint with, but hasn't yet
  // downloaded those, along with information how much data was transferred up
  // to this point and how long that took.
  virtual void PublishCheckinPlanUriReceived(const NetworkStats& network_stats,
                                             absl::Duration phase_duration) = 0;
  // Publishes that the device has finished checking in with the server, along
  // with information how much data was transferred and how long that took.
  virtual void PublishCheckinFinishedV2(const NetworkStats& network_stats,
                                        absl::Duration phase_duration) = 0;
  // Publishes that plan execution has started.
  virtual void PublishComputationStarted() = 0;
  // Publish that the task is invalid.
  virtual void PublishComputationInvalidArgument(
      absl::string_view error_message, const ExampleStats& example_stats,
      const NetworkStats& network_stats, absl::Duration phase_duration) = 0;
  // Publish that an IO error occurred during computation.
  virtual void PublishComputationIOError(absl::string_view error_message,
                                         const ExampleStats& example_stats,
                                         const NetworkStats& network_stats,
                                         absl::Duration phase_duration) = 0;
  // Publish that an example iterator error occurred during computation.
  virtual void PublishComputationExampleIteratorError(
      absl::string_view error_message, const ExampleStats& example_stats,
      const NetworkStats& network_stats, absl::Duration phase_duration) = 0;
  // Publish that an tensorflow error occurred during computation.
  virtual void PublishComputationTensorflowError(
      absl::string_view error_message, const ExampleStats& example_stats,
      const NetworkStats& network_stats, absl::Duration phase_duration) = 0;
  // Publish that the task computation is interrupted.
  virtual void PublishComputationInterrupted(absl::string_view error_message,
                                             const ExampleStats& example_stats,
                                             const NetworkStats& network_stats,
                                             absl::Duration phase_duration) = 0;
  // Publishes an event that plan execution is complete.
  virtual void PublishComputationCompleted(const ExampleStats& example_stats,
                                           const NetworkStats& network_stats,
                                           absl::Duration phase_duration) = 0;
  // Publish that the client starts to upload result.
  virtual void PublishResultUploadStarted() = 0;
  // Publish that an IO error occurred during result upload.
  virtual void PublishResultUploadIOError(absl::string_view error_message,
                                          const NetworkStats& network_stats,
                                          absl::Duration phase_duration) = 0;
  // Publish that the client has interrupted the result upload.
  virtual void PublishResultUploadClientInterrupted(
      absl::string_view error_message, const NetworkStats& network_stats,
      absl::Duration phase_duration) = 0;
  // Publish hat the server has aborted the result upload.
  virtual void PublishResultUploadServerAborted(
      absl::string_view error_message, const NetworkStats& network_stats,
      absl::Duration phase_duration) = 0;
  // Publish that the result upload is completed.
  virtual void PublishResultUploadCompleted(const NetworkStats& network_stats,
                                            absl::Duration phase_duration) = 0;
  // Publish that the task computation has failed, and the client starts to
  // upload the failure to the server.
  virtual void PublishFailureUploadStarted() = 0;
  // Publish that an IO error occurred during failure upload.
  virtual void PublishFailureUploadIOError(absl::string_view error_message,
                                           const NetworkStats& network_stats,
                                           absl::Duration phase_duration) = 0;
  // Publish that the client has interrupted the failure upload.
  virtual void PublishFailureUploadClientInterrupted(
      absl::string_view error_message, const NetworkStats& network_stats,
      absl::Duration phase_duration) = 0;
  // Publish that the server has aborted the failure upload.
  virtual void PublishFailureUploadServerAborted(
      absl::string_view error_message, const NetworkStats& network_stats,
      absl::Duration phase_duration) = 0;
  // Publish that the failure upload completed.
  virtual void PublishFailureUploadCompleted(const NetworkStats& network_stats,
                                             absl::Duration phase_duration) = 0;

  // After calling this function, all subsequently published events will be
  // annotated with the specified model_identifier. This value is typically
  // provided by the federated server and used on events resulting from
  // PublishEligibilityEvalCheckinFinished(), PublishCheckinFinished() and
  // later.
  //
  // Note that this method may be called multiple times with different values,
  // if over the course of a training session multiple models are executed.
  virtual void SetModelIdentifier(const std::string& model_identifier) = 0;

  // Returns a pointer to a publisher which records secure aggregation protocol
  // events.  The returned value must not be nullptr.
  virtual SecAggEventPublisher* secagg_event_publisher() = 0;
};

}  // namespace client
}  // namespace fcp

#endif  // FCP_CLIENT_EVENT_PUBLISHER_H_
