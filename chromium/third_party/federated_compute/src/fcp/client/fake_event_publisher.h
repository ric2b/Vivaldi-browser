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

#ifndef FCP_CLIENT_FAKE_EVENT_PUBLISHER_H_
#define FCP_CLIENT_FAKE_EVENT_PUBLISHER_H_

#include <cxxabi.h>  // IWYU pragma: keep

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "fcp/base/monitoring.h"
#include "fcp/client/event_publisher.h"
#include "fcp/client/secagg_event_publisher.h"
#include "fcp/client/stats.h"

namespace fcp {
namespace client {

class SecAggEventPublisher;

// Macro to print log messages prefixed by ClassName::FunctionName, stripping
// namespaces before ClassName, if any.
#define FCP_CLIENT_LOG_FUNCTION_NAME                              \
  std::string _demangle_buf(1024, '\0');                          \
  size_t _demangle_buf_len = _demangle_buf.length();              \
  abi::__cxa_demangle(typeid(*this).name(), _demangle_buf.data(), \
                      &_demangle_buf_len, nullptr);               \
  FCP_LOG(INFO) << static_cast<std::vector<std::string>>(         \
                       absl::StrSplit(_demangle_buf, "::"))       \
                       .back()                                    \
                       .c_str()                                   \
                << "::" << __func__

// An implementation of the SecAggEventPublisher interface that logs calls to
// stderr.
class SecAggLoggingEventPublisher : public SecAggEventPublisher {
 public:
  explicit SecAggLoggingEventPublisher(bool quiet) : quiet_(quiet) {}

  void PublishStateTransition(::fcp::secagg::ClientState state,
                              size_t last_sent_message_size,
                              size_t last_received_message_size) override {
    if (quiet_) return;
    FCP_CLIENT_LOG_FUNCTION_NAME;
  }
  void PublishError() override { FCP_CLIENT_LOG_FUNCTION_NAME; }
  void PublishAbort(bool client_initiated,
                    const std::string& error_message) override {
    FCP_CLIENT_LOG_FUNCTION_NAME;
  }
  void set_execution_session_id(int64_t execution_session_id) override {
    FCP_CLIENT_LOG_FUNCTION_NAME;
  }

 private:
  const bool quiet_;
};

// An implementation of the EventPublisher interface that logs calls to stderr.
class FakeEventPublisher : public EventPublisher {
 public:
  // Logs all events to stderr.
  FakeEventPublisher() : FakeEventPublisher(/*quiet=*/false) {}
  // Logs only error and "client rejected" events to stderr.
  explicit FakeEventPublisher(bool quiet)
      : quiet_(quiet), secagg_event_publisher_(quiet) {}

  void PublishEligibilityEvalCheckin() override {
    if (quiet_) return;
    FCP_CLIENT_LOG_FUNCTION_NAME;
  }
  void PublishEligibilityEvalPlanUriReceived(const NetworkStats&,
                                             absl::Duration) override {
    if (quiet_) return;
    FCP_CLIENT_LOG_FUNCTION_NAME;
  }

  void PublishEligibilityEvalPlanReceived(const NetworkStats&,
                                          absl::Duration) override {
    if (quiet_) return;
    FCP_CLIENT_LOG_FUNCTION_NAME;
  }

  void PublishEligibilityEvalNotConfigured(const NetworkStats&,
                                           absl::Duration) override {
    if (quiet_) return;
    FCP_CLIENT_LOG_FUNCTION_NAME;
  }

  void PublishEligibilityEvalRejected(const NetworkStats&,
                                      absl::Duration) override {
    FCP_CLIENT_LOG_FUNCTION_NAME;
  }

  void PublishCheckin() override {
    if (quiet_) return;
    FCP_CLIENT_LOG_FUNCTION_NAME;
  }

  void PublishCheckinFinished(const NetworkStats& network_stats,
                              absl::Duration phase_duration) override {
    if (quiet_) return;
    FCP_CLIENT_LOG_FUNCTION_NAME;
  }

  void PublishRejected() override { FCP_CLIENT_LOG_FUNCTION_NAME; }

  void PublishTensorFlowError(int example_count,
                              absl::string_view error_message) override {
    FCP_CLIENT_LOG_FUNCTION_NAME << error_message;
  }

  void PublishIoError(absl::string_view error_message) override {
    FCP_CLIENT_LOG_FUNCTION_NAME << error_message;
  }

  void PublishExampleSelectorError(int example_count,
                                   absl::string_view error_message) override {
    FCP_CLIENT_LOG_FUNCTION_NAME << error_message;
  }

  void PublishInterruption(const ExampleStats& example_stats,
                           absl::Time start_time) override {
    if (quiet_) return;
    FCP_CLIENT_LOG_FUNCTION_NAME;
  }

  void PublishTaskNotStarted(absl::string_view error_message) override {
    FCP_CLIENT_LOG_FUNCTION_NAME << error_message;
  }

  void PublishNonfatalInitializationError(
      absl::string_view error_message) override {
    FCP_CLIENT_LOG_FUNCTION_NAME << error_message;
  }

  void PublishFatalInitializationError(
      absl::string_view error_message) override {
    FCP_CLIENT_LOG_FUNCTION_NAME << error_message;
  }

  void PublishEligibilityEvalCheckinIoError(
      absl::string_view error_message, const NetworkStats& network_stats,
      absl::Duration phase_duration) override {
    FCP_CLIENT_LOG_FUNCTION_NAME << error_message;
  }

  void PublishEligibilityEvalCheckinClientInterrupted(
      absl::string_view error_message, const NetworkStats& network_stats,
      absl::Duration phase_duration) override {
    FCP_CLIENT_LOG_FUNCTION_NAME << error_message;
  }

  void PublishEligibilityEvalCheckinServerAborted(
      absl::string_view error_message, const NetworkStats& network_stats,
      absl::Duration phase_duration) override {
    FCP_CLIENT_LOG_FUNCTION_NAME << error_message;
  }

  void PublishEligibilityEvalCheckinErrorInvalidPayload(
      absl::string_view error_message, const NetworkStats& network_stats,
      absl::Duration phase_duration) override {
    FCP_CLIENT_LOG_FUNCTION_NAME << error_message;
  }

  void PublishEligibilityEvalComputationStarted() override {
    if (quiet_) return;
    FCP_CLIENT_LOG_FUNCTION_NAME;
  }

  void PublishEligibilityEvalComputationInvalidArgument(
      absl::string_view error_message, const ExampleStats& example_stats,
      absl::Duration phase_duration) override {
    FCP_CLIENT_LOG_FUNCTION_NAME << error_message;
  }

  void PublishEligibilityEvalComputationExampleIteratorError(
      absl::string_view error_message, const ExampleStats& example_stats,
      absl::Duration phase_duration) override {
    FCP_CLIENT_LOG_FUNCTION_NAME << error_message;
  }

  void PublishEligibilityEvalComputationTensorflowError(
      absl::string_view error_message, const ExampleStats& example_stats,
      absl::Duration phase_duration) override {
    FCP_CLIENT_LOG_FUNCTION_NAME << error_message;
  }

  void PublishEligibilityEvalComputationIOError(
      absl::string_view error_message, const ExampleStats& example_stats,
      absl::Duration phase_duration) override {
    FCP_CLIENT_LOG_FUNCTION_NAME << error_message;
  }

  void PublishEligibilityEvalComputationInterrupted(
      absl::string_view error_message, const ExampleStats& example_stats,
      absl::Duration phase_duration) override {
    FCP_CLIENT_LOG_FUNCTION_NAME << error_message;
  }

  void PublishEligibilityEvalComputationErrorNonfatal(
      absl::string_view error_message) override {
    FCP_CLIENT_LOG_FUNCTION_NAME << error_message;
  }

  void PublishEligibilityEvalComputationCompleted(
      const ExampleStats& example_stats,
      absl::Duration phase_duration) override {
    if (quiet_) return;
    FCP_CLIENT_LOG_FUNCTION_NAME;
  }

  void PublishMultipleTaskAssignmentsStarted() override {
    FCP_CLIENT_LOG_FUNCTION_NAME;
  }

  void PublishMultipleTaskAssignmentsIOError(
      absl::string_view error_message, const NetworkStats& network_stats,
      absl::Duration phase_duration) override {
    FCP_CLIENT_LOG_FUNCTION_NAME << error_message;
  }

  void PublishMultipleTaskAssignmentsPayloadIOError(
      absl::string_view error_message) override {
    FCP_CLIENT_LOG_FUNCTION_NAME << error_message;
  }

  void PublishMultipleTaskAssignmentsInvalidPayload(
      absl::string_view error_message) override {
    FCP_CLIENT_LOG_FUNCTION_NAME << error_message;
  }

  void PublishMultipleTaskAssignmentsClientInterrupted(
      absl::string_view error_message, const NetworkStats& network_stats,
      absl::Duration phase_duration) override {
    FCP_CLIENT_LOG_FUNCTION_NAME << error_message;
  }

  void PublishMultipleTaskAssignmentsServerAborted(
      absl::string_view error_message, const NetworkStats& network_stats,
      absl::Duration phase_duration) override {
    FCP_CLIENT_LOG_FUNCTION_NAME << error_message;
  }

  void PublishMultipleTaskAssignmentsTurnedAway(
      const NetworkStats& network_stats,
      absl::Duration phase_duration) override {
    FCP_CLIENT_LOG_FUNCTION_NAME;
  }

  void PublishMultipleTaskAssignmentsPlanUriReceived(
      const NetworkStats& network_stats,
      absl::Duration phase_duration) override {
    FCP_CLIENT_LOG_FUNCTION_NAME;
  }

  void PublishMultipleTaskAssignmentsPlanUriPartialReceived(
      const NetworkStats& network_stats,
      absl::Duration phase_duration) override {
    FCP_CLIENT_LOG_FUNCTION_NAME;
  }

  void PublishMultipleTaskAssignmentsPartialCompleted(
      const NetworkStats& network_stats,
      absl::Duration phase_duration) override {
    FCP_CLIENT_LOG_FUNCTION_NAME;
  }

  void PublishMultipleTaskAssignmentsCompleted(
      const NetworkStats& network_stats,
      absl::Duration phase_duration) override {
    FCP_CLIENT_LOG_FUNCTION_NAME;
  }

  void PublishCheckinIoError(absl::string_view error_message,
                             const NetworkStats& network_stats,
                             absl::Duration phase_duration) override {
    FCP_CLIENT_LOG_FUNCTION_NAME << error_message;
  }

  void PublishCheckinClientInterrupted(absl::string_view error_message,
                                       const NetworkStats& network_stats,
                                       absl::Duration phase_duration) override {
    FCP_CLIENT_LOG_FUNCTION_NAME << error_message;
  }

  void PublishCheckinServerAborted(absl::string_view error_message,
                                   const NetworkStats& network_stats,
                                   absl::Duration phase_duration) override {
    FCP_CLIENT_LOG_FUNCTION_NAME << error_message;
  }

  void PublishCheckinInvalidPayload(absl::string_view error_message,
                                    const NetworkStats& network_stats,
                                    absl::Duration phase_duration) override {
    FCP_CLIENT_LOG_FUNCTION_NAME << error_message;
  }

  void PublishRejected(const NetworkStats& network_stats,
                       absl::Duration phase_duration) override {
    FCP_CLIENT_LOG_FUNCTION_NAME;
  }

  void PublishCheckinPlanUriReceived(const NetworkStats& network_stats,
                                     absl::Duration) override {
    if (quiet_) return;
    FCP_CLIENT_LOG_FUNCTION_NAME;
  }
  void PublishCheckinFinishedV2(const NetworkStats& network_stats,
                                absl::Duration phase_duration) override {
    if (quiet_) return;
    FCP_CLIENT_LOG_FUNCTION_NAME;
  }

  void PublishComputationStarted() override {
    if (quiet_) return;
    FCP_CLIENT_LOG_FUNCTION_NAME;
  }

  void PublishComputationInvalidArgument(
      absl::string_view error_message, const ExampleStats& example_stats,
      const NetworkStats& network_stats,
      absl::Duration phase_duration) override {
    FCP_CLIENT_LOG_FUNCTION_NAME << error_message;
  }

  void PublishComputationIOError(absl::string_view error_message,
                                 const ExampleStats& example_stats,
                                 const NetworkStats& network_stats,
                                 absl::Duration phase_duration) override {
    FCP_CLIENT_LOG_FUNCTION_NAME << error_message;
  }

  void PublishComputationExampleIteratorError(
      absl::string_view error_message, const ExampleStats& example_stats,
      const NetworkStats& network_stats,
      absl::Duration phase_duration) override {
    FCP_CLIENT_LOG_FUNCTION_NAME << error_message;
  }

  void PublishComputationTensorflowError(
      absl::string_view error_message, const ExampleStats& example_stats,
      const NetworkStats& network_stats,
      absl::Duration phase_duration) override {
    FCP_CLIENT_LOG_FUNCTION_NAME << error_message;
  }

  void PublishComputationInterrupted(absl::string_view error_message,
                                     const ExampleStats& example_stats,
                                     const NetworkStats& network_stats,
                                     absl::Duration phase_duration) override {
    FCP_CLIENT_LOG_FUNCTION_NAME << error_message;
  }

  void PublishComputationCompleted(const ExampleStats& example_stats,
                                   const NetworkStats& network_stats,
                                   absl::Duration phase_duration) override {
    if (quiet_) return;
    FCP_CLIENT_LOG_FUNCTION_NAME;
  }

  void PublishResultUploadStarted() override {
    if (quiet_) return;
    FCP_CLIENT_LOG_FUNCTION_NAME;
  }

  void PublishResultUploadIOError(absl::string_view error_message,
                                  const NetworkStats& network_stats,
                                  absl::Duration phase_duration) override {
    FCP_CLIENT_LOG_FUNCTION_NAME << error_message;
  }

  void PublishResultUploadClientInterrupted(
      absl::string_view error_message, const NetworkStats& network_stats,
      absl::Duration phase_duration) override {
    FCP_CLIENT_LOG_FUNCTION_NAME << error_message;
  }

  void PublishResultUploadServerAborted(
      absl::string_view error_message, const NetworkStats& network_stats,
      absl::Duration phase_duration) override {
    FCP_CLIENT_LOG_FUNCTION_NAME << error_message;
  }

  void PublishResultUploadCompleted(const NetworkStats& network_stats,
                                    absl::Duration phase_duration) override {
    if (quiet_) return;
    FCP_CLIENT_LOG_FUNCTION_NAME;
  }

  void PublishFailureUploadStarted() override {
    if (quiet_) return;
    FCP_CLIENT_LOG_FUNCTION_NAME;
  }

  void PublishFailureUploadIOError(absl::string_view error_message,
                                   const NetworkStats& network_stats,
                                   absl::Duration phase_duration) override {
    FCP_CLIENT_LOG_FUNCTION_NAME << error_message;
  }

  void PublishFailureUploadClientInterrupted(
      absl::string_view error_message, const NetworkStats& network_stats,
      absl::Duration phase_duration) override {
    FCP_CLIENT_LOG_FUNCTION_NAME << error_message;
  }

  void PublishFailureUploadServerAborted(
      absl::string_view error_message, const NetworkStats& network_stats,
      absl::Duration phase_duration) override {
    FCP_CLIENT_LOG_FUNCTION_NAME << error_message;
  }

  void PublishFailureUploadCompleted(const NetworkStats& network_stats,
                                     absl::Duration phase_duration) override {
    if (quiet_) return;
    FCP_CLIENT_LOG_FUNCTION_NAME;
  }

  void SetModelIdentifier(const std::string& model_identifier) override {
    if (quiet_) return;
    FCP_CLIENT_LOG_FUNCTION_NAME << ":\n\t" << model_identifier;
  }

  SecAggEventPublisher* secagg_event_publisher() override {
    return &secagg_event_publisher_;
  }

 private:
  const bool quiet_;
  SecAggLoggingEventPublisher secagg_event_publisher_;
};

}  // namespace client
}  // namespace fcp

#endif  // FCP_CLIENT_FAKE_EVENT_PUBLISHER_H_
