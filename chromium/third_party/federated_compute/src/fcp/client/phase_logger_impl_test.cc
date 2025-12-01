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
#include <memory>
#include <optional>
#include <string>

#include "google/protobuf/util/time_util.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "fcp/client/diag_codes.pb.h"
#include "fcp/client/engine/engine.pb.h"
#include "fcp/client/histogram_counters.pb.h"
#include "fcp/client/stats.h"
#include "fcp/client/test_helpers.h"
#include "fcp/protos/federated_api.pb.h"
#include "fcp/protos/opstats.pb.h"
#include "fcp/testing/testing.h"

namespace fcp {
namespace client {
namespace {

using ::fcp::client::opstats::OperationalStats;
using ::google::internal::federatedml::v2::RetryWindow;
using ::google::protobuf::util::TimeUtil;
using ::testing::_;
using ::testing::AllOf;
using ::testing::Ge;
using ::testing::InSequence;
using ::testing::Lt;
using ::testing::Return;
using ::testing::StrictMock;

const int64_t kChunkingLayerBytesReceived = 100;
const int64_t kChunkingLayerBytesSent = 50;
const int kTotalExampleCount = 10;
const int64_t kTotalExampleSizeBytes = 1000;

// Parameterize tests with
// 1) whether log tf error message;
// 2) whether separate "plan URI received" events should be logged.
class PhaseLoggerImplTest : public testing::TestWithParam<bool> {
 protected:
  void SetUp() override {
    log_tensorflow_error_messages_ = GetParam();
    EXPECT_CALL(mock_flags_, log_tensorflow_error_messages())
        .WillRepeatedly(Return(log_tensorflow_error_messages_));

    phase_logger_ = std::make_unique<PhaseLoggerImpl>(
        &mock_event_publisher_, &mock_opstats_logger_, &mock_log_manager_,
        &mock_flags_);
  }

  void VerifyCounterLogged(HistogramCounters counter,
                           const testing::Matcher<int64_t>& matcher) {
    EXPECT_CALL(mock_log_manager_,
                LogToLongHistogram(counter, /*execution_index=*/0,
                                   /*epoch_index=*/0,
                                   engine::DataSourceType::DATASET, matcher));
  }

  StrictMock<MockLogManager> mock_log_manager_;
  StrictMock<MockEventPublisher> mock_event_publisher_;
  StrictMock<MockOpStatsLogger> mock_opstats_logger_;
  MockFlags mock_flags_;
  bool log_tensorflow_error_messages_ = false;
  std::unique_ptr<PhaseLoggerImpl> phase_logger_;
  NetworkStats network_stats_ = {
      .bytes_downloaded = kChunkingLayerBytesReceived,
      .bytes_uploaded = kChunkingLayerBytesSent};
  ExampleStats example_stats_ = {.example_count = kTotalExampleCount,
                                 .example_size_bytes = kTotalExampleSizeBytes};
};

std::string GenerateTestName(
    const testing::TestParamInfo<PhaseLoggerImplTest::ParamType>& info) {
  std::string name =
      absl::StrCat(info.param ? "Log_tf_error" : "No_tf_error", "__");
  return name;
}

INSTANTIATE_TEST_SUITE_P(OldVsNewBehavior, PhaseLoggerImplTest, testing::Bool(),
                         GenerateTestName);

TEST_P(PhaseLoggerImplTest, UpdateRetryWindowAndNetworkStats) {
  RetryWindow retry_window;
  *retry_window.mutable_retry_token() = "retry_token";
  *retry_window.mutable_delay_max() = TimeUtil::HoursToDuration(48);
  *retry_window.mutable_delay_min() = TimeUtil::HoursToDuration(4);

  InSequence seq;
  EXPECT_CALL(mock_opstats_logger_, SetRetryWindow(EqualsProto(retry_window)));
  EXPECT_CALL(mock_opstats_logger_,
              SetNetworkStats(testing::Eq(network_stats_)));

  phase_logger_->UpdateRetryWindowAndNetworkStats(retry_window, network_stats_);
}

TEST_P(PhaseLoggerImplTest, SetModelIdentifier) {
  std::string model_identifier = "model_identifier";
  InSequence seq;
  EXPECT_CALL(mock_event_publisher_, SetModelIdentifier(model_identifier));
  EXPECT_CALL(mock_log_manager_, SetModelIdentifier(model_identifier));

  phase_logger_->SetModelIdentifier(model_identifier);
}

TEST_P(PhaseLoggerImplTest, LogTaskNotStarted) {
  std::string error_message = "Client is not ready for training.";
  EXPECT_CALL(mock_event_publisher_, PublishTaskNotStarted(error_message));
  EXPECT_CALL(mock_opstats_logger_,
              AddEventWithErrorMessage(
                  OperationalStats::Event::EVENT_KIND_TRAIN_NOT_STARTED,
                  error_message));
  phase_logger_->LogTaskNotStarted(error_message);
}

TEST_P(PhaseLoggerImplTest, LogEligibilityEvalCheckinStarted) {
  InSequence seq;
  EXPECT_CALL(mock_event_publisher_, PublishEligibilityEvalCheckin());
  EXPECT_CALL(mock_opstats_logger_,
              StartLoggingForPhase(
                  OperationalStats::PhaseStats::ELIGIBILITY_EVAL_CHECKIN));
  EXPECT_CALL(
      mock_opstats_logger_,
      AddEvent(
          OperationalStats::Event::EVENT_KIND_ELIGIBILITY_CHECKIN_STARTED));
  phase_logger_->LogEligibilityEvalCheckinStarted();
}

TEST_P(PhaseLoggerImplTest, LogEligibilityEvalCheckinIOError) {
  std::string error_message = "Network error";
  std::string expected_error_message = absl::StrCat(
      "Error during eligibility check-in: code: 3, error: ", error_message);
  InSequence seq;
  EXPECT_CALL(mock_event_publisher_,
              PublishEligibilityEvalCheckinIoError(expected_error_message,
                                                   network_stats_, _));
  EXPECT_CALL(
      mock_opstats_logger_,
      AddEventWithErrorMessage(
          OperationalStats::Event::EVENT_KIND_ELIGIBILITY_CHECKIN_ERROR_IO,
          expected_error_message));
  EXPECT_CALL(mock_opstats_logger_, StopLoggingForTheCurrentPhase());

  VerifyCounterLogged(
      HistogramCounters::TRAINING_FL_ELIGIBILITY_EVAL_CHECKIN_LATENCY, Ge(0));
  phase_logger_->LogEligibilityEvalCheckinIOError(
      absl::InvalidArgumentError(error_message), network_stats_, absl::Now());
}

TEST_P(PhaseLoggerImplTest, LogCollectionFirstAccessTime) {
  std::string collection_uri = "collection_uri";
  EXPECT_CALL(mock_opstats_logger_,
              RecordCollectionFirstAccessTime(collection_uri, _));

  phase_logger_->MaybeLogCollectionFirstAccessTime(collection_uri);
}

TEST_P(PhaseLoggerImplTest, LogEligibilityEvalCheckinClientInterrupted) {
  std::string error_message = "Client is not idle";
  std::string expected_error_message = absl::StrCat(
      "Error during eligibility check-in: code: 1, error: ", error_message);
  InSequence seq;
  EXPECT_CALL(mock_event_publisher_,
              PublishEligibilityEvalCheckinClientInterrupted(
                  expected_error_message, network_stats_, _));
  EXPECT_CALL(mock_opstats_logger_,
              AddEventWithErrorMessage(
                  OperationalStats::Event::
                      EVENT_KIND_ELIGIBILITY_CHECKIN_CLIENT_INTERRUPTED,
                  expected_error_message));
  EXPECT_CALL(mock_opstats_logger_, StopLoggingForTheCurrentPhase());
  VerifyCounterLogged(
      HistogramCounters::TRAINING_FL_ELIGIBILITY_EVAL_CHECKIN_LATENCY, Ge(0));

  phase_logger_->LogEligibilityEvalCheckinClientInterrupted(
      absl::CancelledError(error_message), network_stats_, absl::Now());
}

TEST_P(PhaseLoggerImplTest, LogEligibilityEvalCheckinServerAborted) {
  std::string error_message = "Connection aborted by the server";
  std::string expected_error_message = absl::StrCat(
      "Error during eligibility check-in: code: 10, error: ", error_message);
  InSequence seq;
  EXPECT_CALL(mock_event_publisher_,
              PublishEligibilityEvalCheckinServerAborted(expected_error_message,
                                                         network_stats_, _));
  EXPECT_CALL(mock_opstats_logger_,
              AddEventWithErrorMessage(
                  OperationalStats::Event::
                      EVENT_KIND_ELIGIBILITY_CHECKIN_SERVER_ABORTED,
                  expected_error_message));
  EXPECT_CALL(mock_opstats_logger_, StopLoggingForTheCurrentPhase());
  VerifyCounterLogged(
      HistogramCounters::TRAINING_FL_ELIGIBILITY_EVAL_CHECKIN_LATENCY, Ge(0));

  phase_logger_->LogEligibilityEvalCheckinServerAborted(
      absl::AbortedError(error_message), network_stats_, absl::Now());
}

TEST_P(PhaseLoggerImplTest, LogEligibilityEvalNotConfigured) {
  InSequence seq;
  EXPECT_CALL(mock_event_publisher_,
              PublishEligibilityEvalNotConfigured(network_stats_, _));
  EXPECT_CALL(
      mock_opstats_logger_,
      AddEvent(OperationalStats::Event::EVENT_KIND_ELIGIBILITY_DISABLED));
  EXPECT_CALL(mock_opstats_logger_, StopLoggingForTheCurrentPhase());
  VerifyCounterLogged(
      HistogramCounters::TRAINING_FL_ELIGIBILITY_EVAL_CHECKIN_LATENCY, Ge(0));

  phase_logger_->LogEligibilityEvalNotConfigured(network_stats_, absl::Now());
}

TEST_P(PhaseLoggerImplTest, LogEligibilityEvalCheckinTurnedAway) {
  InSequence seq;
  EXPECT_CALL(mock_event_publisher_,
              PublishEligibilityEvalRejected(network_stats_, _));
  EXPECT_CALL(
      mock_opstats_logger_,
      AddEvent(OperationalStats::Event::EVENT_KIND_ELIGIBILITY_REJECTED));
  EXPECT_CALL(mock_opstats_logger_, StopLoggingForTheCurrentPhase());
  VerifyCounterLogged(
      HistogramCounters::TRAINING_FL_ELIGIBILITY_EVAL_CHECKIN_LATENCY, Ge(0));

  phase_logger_->LogEligibilityEvalCheckinTurnedAway(network_stats_,
                                                     absl::Now());
}

TEST_P(PhaseLoggerImplTest, LogEligibilityEvalCheckinInvalidPayloadError) {
  std::string error_message = "Cannot parse eligibility eval plan";
  InSequence seq;
  EXPECT_CALL(
      mock_log_manager_,
      LogDiag(
          ProdDiagCode::
              BACKGROUND_TRAINING_ELIGIBILITY_EVAL_FAILED_CANNOT_PARSE_PLAN));
  EXPECT_CALL(mock_event_publisher_,
              PublishEligibilityEvalCheckinErrorInvalidPayload(
                  error_message, network_stats_, _));
  EXPECT_CALL(mock_opstats_logger_,
              AddEventWithErrorMessage(
                  OperationalStats::Event::
                      EVENT_KIND_ELIGIBILITY_CHECKIN_ERROR_INVALID_PAYLOAD,
                  error_message));
  EXPECT_CALL(mock_opstats_logger_, StopLoggingForTheCurrentPhase());
  VerifyCounterLogged(
      HistogramCounters::TRAINING_FL_ELIGIBILITY_EVAL_CHECKIN_LATENCY, Ge(0));

  phase_logger_->LogEligibilityEvalCheckinInvalidPayloadError(
      error_message, network_stats_, absl::Now());
}

TEST_P(PhaseLoggerImplTest, LogEligibilityEvalCheckinPlanUriReceived) {
  EXPECT_CALL(
      mock_opstats_logger_,
      AddEvent(
          OperationalStats::Event::EVENT_KIND_ELIGIBILITY_PLAN_URI_RECEIVED));
  EXPECT_CALL(mock_event_publisher_,
              PublishEligibilityEvalPlanUriReceived(
                  network_stats_,
                  AllOf(Ge(absl::Minutes(1)),
                        Lt(absl::Minutes(1) + absl::Milliseconds(10)))));

  phase_logger_->LogEligibilityEvalCheckinPlanUriReceived(
      network_stats_, absl::Now() - absl::Minutes(1));
}

TEST_P(PhaseLoggerImplTest, LogEligibilityEvalCheckinCompleted) {
  NetworkStats network_stats{.bytes_downloaded = 100,
                             .bytes_uploaded = 200,
                             .network_duration = absl::Seconds(40)};

  absl::Duration expected_duration = absl::Minutes(1);

  InSequence seq;
  EXPECT_CALL(
      mock_opstats_logger_,
      AddEvent(OperationalStats::Event::EVENT_KIND_ELIGIBILITY_ENABLED));
  EXPECT_CALL(mock_opstats_logger_, StopLoggingForTheCurrentPhase());
  EXPECT_CALL(mock_event_publisher_,
              PublishEligibilityEvalPlanReceived(
                  network_stats,
                  AllOf(Ge(expected_duration),
                        Lt(expected_duration + absl::Milliseconds(10)))));
  // The counter should always log the full duration, from before the start of
  // the checkin (regardless of the enable_plan_uri_received_logs_ flag).
  VerifyCounterLogged(
      HistogramCounters::TRAINING_FL_ELIGIBILITY_EVAL_CHECKIN_LATENCY,
      absl::ToInt64Milliseconds(absl::Minutes(2)));

  phase_logger_->LogEligibilityEvalCheckinCompleted(
      network_stats,
      /*time_before_checkin=*/absl::Now() - absl::Minutes(2),
      /*time_before_plan_downloaded=*/absl::Now() - absl::Minutes(1));
}

TEST_P(PhaseLoggerImplTest, LogEligibilityEvalComputationStarted) {
  InSequence seq;
  EXPECT_CALL(mock_event_publisher_,
              PublishEligibilityEvalComputationStarted());
  EXPECT_CALL(mock_opstats_logger_,
              StartLoggingForPhase(
                  OperationalStats::PhaseStats::ELIGIBILITY_COMPUTATION));
  EXPECT_CALL(
      mock_opstats_logger_,
      AddEvent(
          OperationalStats::Event::EVENT_KIND_ELIGIBILITY_COMPUTATION_STARTED));

  phase_logger_->LogEligibilityEvalComputationStarted();
}

TEST_P(PhaseLoggerImplTest, LogEligibilityEvalComputationInvalidArgument) {
  std::string error_message = "Invalid plan.";
  std::string expected_error_message = absl::StrCat(
      "Error during eligibility eval computation: code: 3, error: ",
      error_message);
  InSequence seq;
  EXPECT_CALL(
      mock_log_manager_,
      LogDiag(
          ProdDiagCode::BACKGROUND_TRAINING_FAILED_PLAN_FAILS_SANITY_CHECK));
  EXPECT_CALL(mock_event_publisher_,
              PublishEligibilityEvalComputationInvalidArgument(
                  expected_error_message, example_stats_, _));
  EXPECT_CALL(mock_opstats_logger_,
              AddEventWithErrorMessage(
                  OperationalStats::Event::
                      EVENT_KIND_ELIGIBILITY_COMPUTATION_ERROR_INVALID_ARGUMENT,
                  expected_error_message));
  EXPECT_CALL(mock_opstats_logger_, StopLoggingForTheCurrentPhase());
  phase_logger_->LogEligibilityEvalComputationInvalidArgument(
      absl::InvalidArgumentError(error_message), example_stats_, absl::Now());
}

TEST_P(PhaseLoggerImplTest, LogEligibilityEvalComputationExampleIteratorError) {
  std::string original_message = "Failed to create example iterator";
  absl::Status error_status = absl::InvalidArgumentError(original_message);
  std::string expected_error_message = absl::StrCat(
      "Error during eligibility eval computation: code: 3, error: ",
      original_message);
  InSequence seq;
  EXPECT_CALL(mock_event_publisher_,
              PublishEligibilityEvalComputationExampleIteratorError(
                  expected_error_message, example_stats_, _));
  EXPECT_CALL(mock_opstats_logger_,
              AddEventWithErrorMessage(
                  OperationalStats::Event::
                      EVENT_KIND_ELIGIBILITY_COMPUTATION_ERROR_EXAMPLE_ITERATOR,
                  expected_error_message));
  EXPECT_CALL(mock_opstats_logger_, StopLoggingForTheCurrentPhase());

  phase_logger_->LogEligibilityEvalComputationExampleIteratorError(
      error_status, example_stats_, absl::Now());
}

TEST_P(PhaseLoggerImplTest, LogEligibilityEvalComputationTensorflowError) {
  std::string original_message = "Missing kernel for op NotExist";
  absl::Status error_status = absl::InvalidArgumentError(original_message);
  std::string expected_error_message = absl::StrCat(
      "Error during eligibility eval computation: code: 3, error: ");
  if (log_tensorflow_error_messages_) {
    absl::StrAppend(&expected_error_message, original_message);
  }
  InSequence seq;
  EXPECT_CALL(mock_event_publisher_,
              PublishEligibilityEvalComputationTensorflowError(
                  expected_error_message, example_stats_, _));
  EXPECT_CALL(mock_opstats_logger_,
              AddEventWithErrorMessage(
                  OperationalStats::Event::
                      EVENT_KIND_ELIGIBILITY_COMPUTATION_ERROR_TENSORFLOW,
                  expected_error_message));
  EXPECT_CALL(mock_opstats_logger_, StopLoggingForTheCurrentPhase());
  VerifyCounterLogged(HistogramCounters::TRAINING_RUN_PHASE_LATENCY, Ge(0));
  VerifyCounterLogged(HistogramCounters::TRAINING_RUN_PHASE_END_TIME, Ge(0));
  phase_logger_->LogEligibilityEvalComputationTensorflowError(
      error_status, example_stats_, absl::Now() - absl::Minutes(2),
      absl::Now() - absl::Minutes(5));
}

TEST_P(PhaseLoggerImplTest, LogEligibilityEvalComputationIOError) {
  std::string error_message = "Failed to read opstats dir";
  std::string expected_error_message = absl::StrCat(
      "Error during eligibility eval computation: code: 13, error: ",
      error_message);
  InSequence seq;
  EXPECT_CALL(mock_event_publisher_,
              PublishEligibilityEvalComputationIOError(expected_error_message,
                                                       example_stats_, _));
  EXPECT_CALL(
      mock_opstats_logger_,
      AddEventWithErrorMessage(
          OperationalStats::Event::EVENT_KIND_ELIGIBILITY_COMPUTATION_ERROR_IO,
          expected_error_message));
  EXPECT_CALL(mock_opstats_logger_, StopLoggingForTheCurrentPhase());
  VerifyCounterLogged(HistogramCounters::TRAINING_RUN_PHASE_LATENCY, Ge(0));
  VerifyCounterLogged(HistogramCounters::TRAINING_RUN_PHASE_END_TIME, Ge(0));

  phase_logger_->LogEligibilityEvalComputationIOError(
      absl::InternalError(error_message), example_stats_,
      absl::Now() - absl::Minutes(2), absl::Now() - absl::Minutes(5));
}

TEST_P(PhaseLoggerImplTest, LogEligibilityEvalComputationInterrupted) {
  std::string error_message = "Client is no longer idle";
  std::string expected_error_message = absl::StrCat(
      "Error during eligibility eval computation: code: 1, error: ",
      error_message);
  InSequence seq;
  EXPECT_CALL(mock_event_publisher_,
              PublishEligibilityEvalComputationInterrupted(
                  expected_error_message, example_stats_, _));
  EXPECT_CALL(mock_opstats_logger_,
              AddEventWithErrorMessage(
                  OperationalStats::Event::
                      EVENT_KIND_ELIGIBILITY_COMPUTATION_CLIENT_INTERRUPTED,
                  expected_error_message));
  EXPECT_CALL(mock_opstats_logger_, StopLoggingForTheCurrentPhase());
  VerifyCounterLogged(HistogramCounters::TRAINING_RUN_PHASE_LATENCY, Ge(0));
  VerifyCounterLogged(HistogramCounters::TRAINING_RUN_PHASE_END_TIME, Ge(0));

  phase_logger_->LogEligibilityEvalComputationInterrupted(
      absl::CancelledError(error_message), example_stats_,
      absl::Now() - absl::Minutes(2), absl::Now() - absl::Minutes(5));
}

TEST_P(PhaseLoggerImplTest, LogEligibilityEvalComputationCompleted) {
  absl::Time run_plan_start_time = absl::Now() - absl::Minutes(8);
  absl::Time reference_time = absl::Now() - absl::Minutes(9);
  InSequence seq;
  EXPECT_CALL(mock_event_publisher_,
              PublishEligibilityEvalComputationCompleted(example_stats_, _));
  EXPECT_CALL(mock_opstats_logger_,
              AddEvent(OperationalStats::Event::
                           EVENT_KIND_ELIGIBILITY_COMPUTATION_FINISHED));
  EXPECT_CALL(mock_opstats_logger_, StopLoggingForTheCurrentPhase());
  VerifyCounterLogged(HistogramCounters::TRAINING_OVERALL_EXAMPLE_SIZE,
                      kTotalExampleSizeBytes);
  VerifyCounterLogged(HistogramCounters::TRAINING_OVERALL_EXAMPLE_COUNT,
                      kTotalExampleCount);
  VerifyCounterLogged(HistogramCounters::TRAINING_RUN_PHASE_LATENCY, Ge(0));
  VerifyCounterLogged(HistogramCounters::TRAINING_RUN_PHASE_END_TIME, Ge(0));

  phase_logger_->LogEligibilityEvalComputationCompleted(
      example_stats_, run_plan_start_time, reference_time);
}

TEST_P(PhaseLoggerImplTest, LogCheckinStarted) {
  InSequence seq;
  EXPECT_CALL(mock_event_publisher_, PublishCheckin());
  EXPECT_CALL(mock_opstats_logger_,
              StartLoggingForPhase(OperationalStats::PhaseStats::CHECKIN));
  EXPECT_CALL(mock_opstats_logger_,
              AddEvent(OperationalStats::Event::EVENT_KIND_CHECKIN_STARTED));
  phase_logger_->LogCheckinStarted();
}

TEST_P(PhaseLoggerImplTest, LogCheckinIOError) {
  std::string error_message = "IO error";
  std::string expected_error_message =
      absl::StrCat("Error during check-in: code: 14, error: ", error_message);
  InSequence seq;
  EXPECT_CALL(mock_event_publisher_,
              PublishCheckinIoError(expected_error_message, network_stats_, _));
  EXPECT_CALL(mock_opstats_logger_,
              AddEventWithErrorMessage(
                  OperationalStats::Event::EVENT_KIND_CHECKIN_ERROR_IO,
                  expected_error_message));
  EXPECT_CALL(mock_opstats_logger_, StopLoggingForTheCurrentPhase());
  VerifyCounterLogged(HistogramCounters::TRAINING_FL_CHECKIN_LATENCY, Ge(0));
  VerifyCounterLogged(HistogramCounters::TRAINING_FL_CHECKIN_END_TIME, Ge(0));

  phase_logger_->LogCheckinIOError(
      absl::UnavailableError(error_message), network_stats_,
      absl::Now() - absl::Minutes(2), absl::Now() - absl::Minutes(8));
}

TEST_P(PhaseLoggerImplTest, LogCheckinClientInterrupted) {
  std::string error_message = "The client is no longer idle";
  std::string expected_error_message =
      absl::StrCat("Error during check-in: code: 1, error: ", error_message);
  InSequence seq;
  EXPECT_CALL(mock_event_publisher_,
              PublishCheckinClientInterrupted(expected_error_message,
                                              network_stats_, _));
  EXPECT_CALL(
      mock_opstats_logger_,
      AddEventWithErrorMessage(
          OperationalStats::Event::EVENT_KIND_CHECKIN_CLIENT_INTERRUPTED,
          expected_error_message));
  EXPECT_CALL(mock_opstats_logger_, StopLoggingForTheCurrentPhase());
  VerifyCounterLogged(HistogramCounters::TRAINING_FL_CHECKIN_LATENCY, Ge(0));
  VerifyCounterLogged(HistogramCounters::TRAINING_FL_CHECKIN_END_TIME, Ge(0));

  phase_logger_->LogCheckinClientInterrupted(
      absl::CancelledError(error_message), network_stats_,
      absl::Now() - absl::Minutes(2), absl::Now() - absl::Minutes(8));
}

TEST_P(PhaseLoggerImplTest, LogCheckinServerAborted) {
  std::string error_message = "The request is aborted by the server";
  std::string expected_error_message =
      absl::StrCat("Error during check-in: code: 10, error: ", error_message);
  InSequence seq;
  EXPECT_CALL(
      mock_event_publisher_,
      PublishCheckinServerAborted(expected_error_message, network_stats_, _));
  EXPECT_CALL(mock_opstats_logger_,
              AddEventWithErrorMessage(
                  OperationalStats::Event::EVENT_KIND_CHECKIN_SERVER_ABORTED,
                  expected_error_message));
  EXPECT_CALL(mock_opstats_logger_, StopLoggingForTheCurrentPhase());

  VerifyCounterLogged(HistogramCounters::TRAINING_FL_CHECKIN_LATENCY, Ge(0));
  VerifyCounterLogged(HistogramCounters::TRAINING_FL_CHECKIN_END_TIME, Ge(0));

  phase_logger_->LogCheckinServerAborted(
      absl::AbortedError(error_message), network_stats_,
      absl::Now() - absl::Minutes(2), absl::Now() - absl::Minutes(8));
}

TEST_P(PhaseLoggerImplTest, LogCheckinTurnedAway) {
  InSequence seq;
  EXPECT_CALL(mock_event_publisher_, PublishRejected(network_stats_, _));
  EXPECT_CALL(mock_opstats_logger_,
              AddEvent(OperationalStats::Event::EVENT_KIND_CHECKIN_REJECTED));
  EXPECT_CALL(mock_opstats_logger_, StopLoggingForTheCurrentPhase());
  VerifyCounterLogged(HistogramCounters::TRAINING_FL_CHECKIN_LATENCY, Ge(0));
  VerifyCounterLogged(HistogramCounters::TRAINING_FL_CHECKIN_END_TIME, Ge(0));

  phase_logger_->LogCheckinTurnedAway(network_stats_,
                                      absl::Now() - absl::Minutes(2),
                                      absl::Now() - absl::Minutes(8));
}

TEST_P(PhaseLoggerImplTest, LogCheckinInvalidPayload) {
  std::string error_message = "Cannot parse plan";
  InSequence seq;
  EXPECT_CALL(
      mock_log_manager_,
      LogDiag(ProdDiagCode::BACKGROUND_TRAINING_FAILED_CANNOT_PARSE_PLAN));
  EXPECT_CALL(mock_event_publisher_,
              PublishCheckinInvalidPayload(error_message, network_stats_, _));
  EXPECT_CALL(
      mock_opstats_logger_,
      AddEventWithErrorMessage(
          OperationalStats::Event::EVENT_KIND_CHECKIN_ERROR_INVALID_PAYLOAD,
          error_message));
  EXPECT_CALL(mock_opstats_logger_, StopLoggingForTheCurrentPhase());
  VerifyCounterLogged(HistogramCounters::TRAINING_FL_CHECKIN_LATENCY, Ge(0));
  VerifyCounterLogged(HistogramCounters::TRAINING_FL_CHECKIN_END_TIME, Ge(0));

  phase_logger_->LogCheckinInvalidPayload(error_message, network_stats_,
                                          absl::Now() - absl::Minutes(2),
                                          absl::Now() - absl::Minutes(8));
}

TEST_P(PhaseLoggerImplTest, LogCheckinPlanUriReceived) {
  std::string task_name = "my_task";
  EXPECT_CALL(mock_event_publisher_,
              PublishCheckinPlanUriReceived(
                  network_stats_,
                  AllOf(Ge(absl::Minutes(1)),
                        Lt(absl::Minutes(1) + absl::Milliseconds(10)))));
  EXPECT_CALL(
      mock_opstats_logger_,
      AddEventAndSetTaskName(
          task_name,
          OperationalStats::Event::EVENT_KIND_CHECKIN_PLAN_URI_RECEIVED));

  phase_logger_->LogCheckinPlanUriReceived(task_name, network_stats_,
                                           absl::Now() - absl::Minutes(1));
}

TEST_P(PhaseLoggerImplTest, LogCheckinCompleted) {
  NetworkStats network_stats{.bytes_downloaded = 100,
                             .bytes_uploaded = 200,
                             .network_duration = absl::Seconds(40)};

  absl::Duration expected_duration = absl::Minutes(1);

  std::string task_name = "my_task";
  InSequence seq;
  EXPECT_CALL(mock_event_publisher_,
              PublishCheckinFinishedV2(
                  network_stats,
                  AllOf(Ge(expected_duration),
                        Lt(expected_duration + absl::Milliseconds(10)))));
  EXPECT_CALL(mock_opstats_logger_,
              AddEvent(OperationalStats::Event::EVENT_KIND_CHECKIN_ACCEPTED));
  EXPECT_CALL(mock_opstats_logger_, StopLoggingForTheCurrentPhase());
  // The counter should always log the full duration, from before the start of
  // the checkin.
  VerifyCounterLogged(HistogramCounters::TRAINING_FL_CHECKIN_LATENCY,
                      AllOf(Ge(absl::ToInt64Milliseconds(absl::Minutes(2))),
                            Lt(absl::ToInt64Milliseconds(
                                absl::Minutes(2) + absl::Milliseconds(100)))));
  VerifyCounterLogged(HistogramCounters::TRAINING_FL_CHECKIN_END_TIME,
                      AllOf(Ge(absl::ToInt64Milliseconds(absl::Minutes(8))),
                            Lt(absl::ToInt64Milliseconds(
                                absl::Minutes(8) + absl::Milliseconds(100)))));

  phase_logger_->LogCheckinCompleted(
      task_name, network_stats,
      /*time_before_checkin=*/absl::Now() - absl::Minutes(2),
      /*time_before_plan_download=*/absl::Now() - absl::Minutes(1),
      /*reference_time=*/absl::Now() - absl::Minutes(8));
}

TEST_P(PhaseLoggerImplTest, LogComputationStarted) {
  InSequence seq;
  EXPECT_CALL(mock_event_publisher_, PublishComputationStarted());
  EXPECT_CALL(mock_opstats_logger_,
              StartLoggingForPhase(OperationalStats::PhaseStats::COMPUTATION));
  const std::string task_name = "my_task";
  EXPECT_CALL(
      mock_opstats_logger_,
      AddEventAndSetTaskName(
          task_name, OperationalStats::Event::EVENT_KIND_COMPUTATION_STARTED));
  phase_logger_->LogComputationStarted(task_name);
}

TEST_P(PhaseLoggerImplTest, LogComputationInvalidArgument) {
  std::string error_message = "Unexpected input tensor";
  std::string expected_error_message =
      absl::StrCat("Error during computation: code: 3, error: ", error_message);
  InSequence seq;
  EXPECT_CALL(
      mock_log_manager_,
      LogDiag(
          ProdDiagCode::BACKGROUND_TRAINING_FAILED_PLAN_FAILS_SANITY_CHECK));
  EXPECT_CALL(mock_event_publisher_,
              PublishComputationInvalidArgument(
                  expected_error_message, example_stats_, network_stats_, _));
  EXPECT_CALL(mock_opstats_logger_,
              AddEventWithErrorMessage(
                  OperationalStats::Event::
                      EVENT_KIND_COMPUTATION_ERROR_INVALID_ARGUMENT,
                  expected_error_message));
  EXPECT_CALL(mock_opstats_logger_, StopLoggingForTheCurrentPhase());
  phase_logger_->LogComputationInvalidArgument(
      absl::InvalidArgumentError(error_message), example_stats_, network_stats_,
      absl::Now());
}

TEST_P(PhaseLoggerImplTest, LogComputationIOError) {
  std::string error_message = "IO error";
  std::string expected_error_message =
      absl::StrCat("Error during computation: code: 3, error: ", error_message);
  InSequence seq;
  EXPECT_CALL(mock_event_publisher_,
              PublishComputationIOError(expected_error_message, example_stats_,
                                        network_stats_, _));
  EXPECT_CALL(mock_opstats_logger_,
              AddEventWithErrorMessage(
                  OperationalStats::Event::EVENT_KIND_COMPUTATION_ERROR_IO,
                  expected_error_message));
  EXPECT_CALL(mock_opstats_logger_, StopLoggingForTheCurrentPhase());
  phase_logger_->LogComputationIOError(
      absl::InvalidArgumentError(error_message), example_stats_, network_stats_,
      absl::Now());
}

TEST_P(PhaseLoggerImplTest, LogComputationExampleIteratorError) {
  std::string original_message = "Cannot create example iterator";
  absl::Status error_status = absl::InvalidArgumentError(original_message);
  std::string expected_error_message = absl::StrCat(
      "Error during computation: code: 3, error: ", original_message);
  InSequence seq;
  EXPECT_CALL(mock_event_publisher_,
              PublishComputationExampleIteratorError(
                  expected_error_message, example_stats_, network_stats_, _));
  EXPECT_CALL(mock_opstats_logger_,
              AddEventWithErrorMessage(
                  OperationalStats::Event::
                      EVENT_KIND_COMPUTATION_ERROR_EXAMPLE_ITERATOR,
                  expected_error_message));
  EXPECT_CALL(mock_opstats_logger_, StopLoggingForTheCurrentPhase());
  phase_logger_->LogComputationExampleIteratorError(
      error_status, example_stats_, network_stats_, absl::Now());
}

TEST_P(PhaseLoggerImplTest, LogComputationTensorflowError) {
  std::string original_message = "Missing op kernel NotExist";
  absl::Status error_status = absl::InvalidArgumentError(original_message);
  std::string expected_error_message =
      absl::StrCat("Error during computation: code: 3, error: ");
  if (log_tensorflow_error_messages_) {
    absl::StrAppend(&expected_error_message, original_message);
  }
  InSequence seq;
  EXPECT_CALL(mock_event_publisher_,
              PublishComputationTensorflowError(
                  expected_error_message, example_stats_, network_stats_, _));
  EXPECT_CALL(
      mock_opstats_logger_,
      AddEventWithErrorMessage(
          OperationalStats::Event::EVENT_KIND_COMPUTATION_ERROR_TENSORFLOW,
          expected_error_message));
  EXPECT_CALL(mock_opstats_logger_, StopLoggingForTheCurrentPhase());
  VerifyCounterLogged(HistogramCounters::TRAINING_RUN_PHASE_LATENCY, Ge(0));
  VerifyCounterLogged(HistogramCounters::TRAINING_RUN_PHASE_END_TIME, Ge(0));
  phase_logger_->LogComputationTensorflowError(
      error_status, example_stats_, network_stats_,
      absl::Now() - absl::Minutes(6), absl::Now() - absl::Minutes(8));
}

TEST_P(PhaseLoggerImplTest, LogComputationInterrupted) {
  absl::Time run_plan_start_time = absl::Now() - absl::Minutes(6);
  absl::Time reference_time = absl::Now() - absl::Minutes(8);
  std::string error_message = "Client is no longer idle.";
  std::string expected_error_message =
      absl::StrCat("Error during computation: code: 1, error: ", error_message);
  InSequence seq;
  EXPECT_CALL(mock_event_publisher_,
              PublishComputationInterrupted(expected_error_message,
                                            example_stats_, network_stats_, _));
  EXPECT_CALL(
      mock_opstats_logger_,
      AddEventWithErrorMessage(
          OperationalStats::Event::EVENT_KIND_COMPUTATION_CLIENT_INTERRUPTED,
          expected_error_message));
  EXPECT_CALL(mock_opstats_logger_, StopLoggingForTheCurrentPhase());

  VerifyCounterLogged(HistogramCounters::TRAINING_RUN_PHASE_LATENCY, Ge(0));
  VerifyCounterLogged(HistogramCounters::TRAINING_RUN_PHASE_END_TIME, Ge(0));

  phase_logger_->LogComputationInterrupted(absl::CancelledError(error_message),
                                           example_stats_, network_stats_,
                                           run_plan_start_time, reference_time);
}

TEST_P(PhaseLoggerImplTest, LogComputationCompleted) {
  absl::Time run_plan_start_time = absl::Now() - absl::Minutes(6);
  absl::Time reference_time = absl::Now() - absl::Minutes(8);
  InSequence seq;
  EXPECT_CALL(mock_event_publisher_,
              PublishComputationCompleted(example_stats_, network_stats_,
                                          Ge(absl::Minutes(6))));
  EXPECT_CALL(
      mock_opstats_logger_,
      AddEvent(OperationalStats::Event::EVENT_KIND_COMPUTATION_FINISHED));
  EXPECT_CALL(mock_opstats_logger_, StopLoggingForTheCurrentPhase());
  VerifyCounterLogged(HistogramCounters::TRAINING_OVERALL_EXAMPLE_SIZE,
                      kTotalExampleSizeBytes);
  VerifyCounterLogged(HistogramCounters::TRAINING_OVERALL_EXAMPLE_COUNT,
                      kTotalExampleCount);
  VerifyCounterLogged(HistogramCounters::TRAINING_RUN_PHASE_LATENCY,
                      Ge(absl::ToInt64Milliseconds(absl::Minutes(6))));
  VerifyCounterLogged(HistogramCounters::TRAINING_RUN_PHASE_END_TIME,
                      Ge(absl::ToInt64Milliseconds(absl::Minutes(8))));

  phase_logger_->LogComputationCompleted(example_stats_, network_stats_,
                                         run_plan_start_time, reference_time,
                                         /*min_sep_policy_index=*/std::nullopt);
}

TEST_P(PhaseLoggerImplTest, LogComputationCompletedWithMinSepPolicyIndex) {
  absl::Time run_plan_start_time = absl::Now() - absl::Minutes(6);
  absl::Time reference_time = absl::Now() - absl::Minutes(8);
  std::optional<int> min_sep_policy_index = 1;
  InSequence seq;
  EXPECT_CALL(mock_event_publisher_,
              PublishComputationCompleted(example_stats_, network_stats_,
                                          Ge(absl::Minutes(6))));
  EXPECT_CALL(
      mock_opstats_logger_,
      AddEvent(OperationalStats::Event::EVENT_KIND_COMPUTATION_FINISHED));
  EXPECT_CALL(mock_opstats_logger_,
              SetMinSepPolicyIndex(min_sep_policy_index.value()));
  EXPECT_CALL(mock_opstats_logger_, StopLoggingForTheCurrentPhase());
  VerifyCounterLogged(HistogramCounters::TRAINING_OVERALL_EXAMPLE_SIZE,
                      kTotalExampleSizeBytes);
  VerifyCounterLogged(HistogramCounters::TRAINING_OVERALL_EXAMPLE_COUNT,
                      kTotalExampleCount);
  VerifyCounterLogged(HistogramCounters::TRAINING_RUN_PHASE_LATENCY,
                      Ge(absl::ToInt64Milliseconds(absl::Minutes(6))));
  VerifyCounterLogged(HistogramCounters::TRAINING_RUN_PHASE_END_TIME,
                      Ge(absl::ToInt64Milliseconds(absl::Minutes(8))));

  phase_logger_->LogComputationCompleted(example_stats_, network_stats_,
                                         run_plan_start_time, reference_time,
                                         min_sep_policy_index);
}

TEST_P(PhaseLoggerImplTest, LogResultUploadStartedOpStatsDbCommitSucceeds) {
  InSequence seq;
  EXPECT_CALL(mock_opstats_logger_,
              StartLoggingForPhase(OperationalStats::PhaseStats::UPLOAD));
  EXPECT_CALL(
      mock_opstats_logger_,
      AddEvent(OperationalStats::Event::EVENT_KIND_RESULT_UPLOAD_STARTED));
  EXPECT_CALL(mock_opstats_logger_, CommitToStorage)
      .WillOnce(Return(absl::OkStatus()));
  EXPECT_CALL(mock_event_publisher_, PublishResultUploadStarted());

  ASSERT_OK(phase_logger_->LogResultUploadStarted());
}

TEST_P(PhaseLoggerImplTest, LogResultUploadStartedOpStatsDbCommitFails) {
  InSequence seq;
  EXPECT_CALL(mock_opstats_logger_,
              StartLoggingForPhase(OperationalStats::PhaseStats::UPLOAD));
  EXPECT_CALL(
      mock_opstats_logger_,
      AddEvent(OperationalStats::Event::EVENT_KIND_RESULT_UPLOAD_STARTED));
  EXPECT_CALL(mock_opstats_logger_, CommitToStorage)
      .WillOnce(Return(absl::InternalError("")));

  ASSERT_THAT(phase_logger_->LogResultUploadStarted(),
              IsCode(absl::StatusCode::kInternal));
}

TEST_P(PhaseLoggerImplTest, LogResultUploadIOError) {
  std::string error_message = "Network IO";
  std::string expected_error_message =
      absl::StrCat("Error reporting results: code: 14, error: ", error_message);
  InSequence seq;
  EXPECT_CALL(
      mock_event_publisher_,
      PublishResultUploadIOError(expected_error_message, network_stats_, _));
  EXPECT_CALL(mock_opstats_logger_,
              AddEventWithErrorMessage(
                  OperationalStats::Event::EVENT_KIND_RESULT_UPLOAD_ERROR_IO,
                  expected_error_message));
  EXPECT_CALL(mock_opstats_logger_, StopLoggingForTheCurrentPhase());
  VerifyCounterLogged(HistogramCounters::TRAINING_FL_REPORT_RESULTS_LATENCY,
                      Ge(0));
  VerifyCounterLogged(HistogramCounters::TRAINING_FL_REPORT_RESULTS_END_TIME,
                      Ge(0));
  phase_logger_->LogResultUploadIOError(
      absl::UnavailableError(error_message), network_stats_,
      absl::Now() - absl::Minutes(1), absl::Now() - absl::Minutes(8));
}

TEST_P(PhaseLoggerImplTest, LogResultUploadClientInterrupted) {
  std::string error_message = "Client is no longer idle";
  std::string expected_error_message =
      absl::StrCat("Error reporting results: code: 1, error: ", error_message);
  InSequence seq;
  EXPECT_CALL(mock_event_publisher_,
              PublishResultUploadClientInterrupted(expected_error_message,
                                                   network_stats_, _));
  EXPECT_CALL(
      mock_opstats_logger_,
      AddEventWithErrorMessage(
          OperationalStats::Event::EVENT_KIND_RESULT_UPLOAD_CLIENT_INTERRUPTED,
          expected_error_message));
  EXPECT_CALL(mock_opstats_logger_, StopLoggingForTheCurrentPhase());
  VerifyCounterLogged(HistogramCounters::TRAINING_FL_REPORT_RESULTS_LATENCY,
                      Ge(0));
  VerifyCounterLogged(HistogramCounters::TRAINING_FL_REPORT_RESULTS_END_TIME,
                      Ge(0));

  phase_logger_->LogResultUploadClientInterrupted(
      absl::CancelledError(error_message), network_stats_,
      absl::Now() - absl::Minutes(1), absl::Now() - absl::Minutes(8));
}

TEST_P(PhaseLoggerImplTest, LogResultUploadServerAborted) {
  std::string error_message = "Request is aborted by the server";
  std::string expected_error_message =
      absl::StrCat("Error reporting results: code: 10, error: ", error_message);
  InSequence seq;
  EXPECT_CALL(mock_event_publisher_,
              PublishResultUploadServerAborted(expected_error_message,
                                               network_stats_, _));
  EXPECT_CALL(
      mock_opstats_logger_,
      AddEventWithErrorMessage(
          OperationalStats::Event::EVENT_KIND_RESULT_UPLOAD_SERVER_ABORTED,
          expected_error_message));
  EXPECT_CALL(mock_opstats_logger_, StopLoggingForTheCurrentPhase());
  VerifyCounterLogged(HistogramCounters::TRAINING_FL_REPORT_RESULTS_LATENCY,
                      Ge(0));
  VerifyCounterLogged(HistogramCounters::TRAINING_FL_REPORT_RESULTS_END_TIME,
                      Ge(0));

  phase_logger_->LogResultUploadServerAborted(
      absl::AbortedError(error_message), network_stats_,
      absl::Now() - absl::Minutes(1), absl::Now() - absl::Minutes(8));
}

TEST_P(PhaseLoggerImplTest, LogResultUploadCompleted) {
  InSequence seq;
  EXPECT_CALL(mock_event_publisher_,
              PublishResultUploadCompleted(network_stats_, _));
  EXPECT_CALL(
      mock_opstats_logger_,
      AddEvent(OperationalStats::Event::EVENT_KIND_RESULT_UPLOAD_FINISHED));
  EXPECT_CALL(mock_opstats_logger_, StopLoggingForTheCurrentPhase());
  VerifyCounterLogged(HistogramCounters::TRAINING_FL_REPORT_RESULTS_LATENCY,
                      Ge(0));
  VerifyCounterLogged(HistogramCounters::TRAINING_FL_REPORT_RESULTS_END_TIME,
                      Ge(0));

  phase_logger_->LogResultUploadCompleted(network_stats_,
                                          absl::Now() - absl::Minutes(1),
                                          absl::Now() - absl::Minutes(8));
}

TEST_P(PhaseLoggerImplTest, LogFailureUploadStartedOpstatsDbCommitSucceeds) {
  InSequence seq;
  EXPECT_CALL(mock_opstats_logger_,
              StartLoggingForPhase(OperationalStats::PhaseStats::UPLOAD));
  EXPECT_CALL(
      mock_opstats_logger_,
      AddEvent(OperationalStats::Event::EVENT_KIND_FAILURE_UPLOAD_STARTED));
  EXPECT_CALL(mock_opstats_logger_, CommitToStorage())
      .WillOnce(Return(absl::OkStatus()));
  EXPECT_CALL(mock_event_publisher_, PublishFailureUploadStarted());
  ASSERT_OK(phase_logger_->LogFailureUploadStarted());
}

TEST_P(PhaseLoggerImplTest, LogFailureUploadStartedOpstatsDbCommitFails) {
  InSequence seq;
  EXPECT_CALL(mock_opstats_logger_,
              StartLoggingForPhase(OperationalStats::PhaseStats::UPLOAD));
  EXPECT_CALL(
      mock_opstats_logger_,
      AddEvent(OperationalStats::Event::EVENT_KIND_FAILURE_UPLOAD_STARTED));
  EXPECT_CALL(mock_opstats_logger_, CommitToStorage())
      .WillOnce(Return(absl::InternalError("")));
  ASSERT_THAT(phase_logger_->LogFailureUploadStarted(),
              IsCode(absl::StatusCode::kInternal));
}

TEST_P(PhaseLoggerImplTest, LogFailureUploadIOError) {
  std::string error_message = "Network error.";
  std::string expected_error_message = absl::StrCat(
      "Error reporting computation failure: code: 14, error: ", error_message);
  InSequence seq;
  EXPECT_CALL(
      mock_event_publisher_,
      PublishFailureUploadIOError(expected_error_message, network_stats_, _));
  EXPECT_CALL(mock_opstats_logger_,
              AddEventWithErrorMessage(
                  OperationalStats::Event::EVENT_KIND_FAILURE_UPLOAD_ERROR_IO,
                  expected_error_message));
  EXPECT_CALL(mock_opstats_logger_, StopLoggingForTheCurrentPhase());
  VerifyCounterLogged(HistogramCounters::TRAINING_FL_REPORT_RESULTS_LATENCY,
                      Ge(0));
  VerifyCounterLogged(HistogramCounters::TRAINING_FL_REPORT_RESULTS_END_TIME,
                      Ge(0));
  phase_logger_->LogFailureUploadIOError(
      absl::UnavailableError(error_message), network_stats_,
      absl::Now() - absl::Minutes(1), absl::Now() - absl::Minutes(8));
}

TEST_P(PhaseLoggerImplTest, LogFailureUploadClientInterrupted) {
  std::string error_message = "The client is no longer idle";
  std::string expected_error_message = absl::StrCat(
      "Error reporting computation failure: code: 1, error: ", error_message);
  InSequence seq;
  EXPECT_CALL(mock_event_publisher_,
              PublishFailureUploadClientInterrupted(expected_error_message,
                                                    network_stats_, _));
  EXPECT_CALL(
      mock_opstats_logger_,
      AddEventWithErrorMessage(
          OperationalStats::Event::EVENT_KIND_FAILURE_UPLOAD_CLIENT_INTERRUPTED,
          expected_error_message));
  EXPECT_CALL(mock_opstats_logger_, StopLoggingForTheCurrentPhase());
  VerifyCounterLogged(HistogramCounters::TRAINING_FL_REPORT_RESULTS_LATENCY,
                      Ge(0));
  VerifyCounterLogged(HistogramCounters::TRAINING_FL_REPORT_RESULTS_END_TIME,
                      Ge(0));
  phase_logger_->LogFailureUploadClientInterrupted(
      absl::CancelledError(error_message), network_stats_,
      absl::Now() - absl::Minutes(1), absl::Now() - absl::Minutes(8));
}

TEST_P(PhaseLoggerImplTest, LogFailureUploadServerAborted) {
  std::string error_message = "Request is aborted by the server.";
  std::string expected_error_message = absl::StrCat(
      "Error reporting computation failure: code: 10, error: ", error_message);
  InSequence seq;
  EXPECT_CALL(mock_event_publisher_,
              PublishFailureUploadServerAborted(expected_error_message,
                                                network_stats_, _));
  EXPECT_CALL(
      mock_opstats_logger_,
      AddEventWithErrorMessage(
          OperationalStats::Event::EVENT_KIND_FAILURE_UPLOAD_SERVER_ABORTED,
          expected_error_message));
  EXPECT_CALL(mock_opstats_logger_, StopLoggingForTheCurrentPhase());
  VerifyCounterLogged(HistogramCounters::TRAINING_FL_REPORT_RESULTS_LATENCY,
                      Ge(0));
  VerifyCounterLogged(HistogramCounters::TRAINING_FL_REPORT_RESULTS_END_TIME,
                      Ge(0));
  phase_logger_->LogFailureUploadServerAborted(
      absl::AbortedError(error_message), network_stats_,
      absl::Now() - absl::Minutes(1), absl::Now() - absl::Minutes(8));
}

TEST_P(PhaseLoggerImplTest, LogFailureUploadCompleted) {
  InSequence seq;
  EXPECT_CALL(mock_event_publisher_,
              PublishFailureUploadCompleted(network_stats_, _));
  EXPECT_CALL(
      mock_opstats_logger_,
      AddEvent(OperationalStats::Event::EVENT_KIND_FAILURE_UPLOAD_FINISHED));
  EXPECT_CALL(mock_opstats_logger_, StopLoggingForTheCurrentPhase());
  VerifyCounterLogged(HistogramCounters::TRAINING_FL_REPORT_RESULTS_LATENCY,
                      Ge(0));
  VerifyCounterLogged(HistogramCounters::TRAINING_FL_REPORT_RESULTS_END_TIME,
                      Ge(0));
  phase_logger_->LogFailureUploadCompleted(network_stats_,
                                           absl::Now() - absl::Minutes(1),
                                           absl::Now() - absl::Minutes(8));
}

TEST_P(PhaseLoggerImplTest, LogMultipleTaskAssignmentsStarted) {
  InSequence seq;
  EXPECT_CALL(mock_event_publisher_, PublishMultipleTaskAssignmentsStarted());
  EXPECT_CALL(mock_opstats_logger_,
              StartLoggingForPhase(
                  OperationalStats::PhaseStats::MULTIPLE_TASK_ASSIGNMENTS));
  EXPECT_CALL(mock_opstats_logger_,
              AddEvent(OperationalStats::Event::
                           EVENT_KIND_MULTIPLE_TASK_ASSIGNMENTS_STARTED));
  phase_logger_->LogMultipleTaskAssignmentsStarted();
}

TEST_P(PhaseLoggerImplTest, LogMultipleTaskAssignmentsIOError) {
  InSequence seq;
  std::string error_message = "Something's gone wrong.";
  std::string expected_error_message =
      absl::StrCat("Error during multiple task assignments: code: 13, error: ",
                   error_message);
  EXPECT_CALL(mock_event_publisher_,
              PublishMultipleTaskAssignmentsIOError(expected_error_message,
                                                    network_stats_, _));
  EXPECT_CALL(mock_opstats_logger_,
              AddEventWithErrorMessage(
                  OperationalStats::Event::
                      EVENT_KIND_MULTIPLE_TASK_ASSIGNMENTS_ERROR_IO,
                  expected_error_message));
  EXPECT_CALL(mock_opstats_logger_, StopLoggingForTheCurrentPhase());
  VerifyCounterLogged(
      HistogramCounters::TRAINING_FL_MULTIPLE_TASK_ASSIGNMENTS_LATENCY, Ge(0));
  VerifyCounterLogged(
      HistogramCounters::TRAINING_FL_MULTIPLE_TASK_ASSIGNMENTS_END_TIME, Ge(0));
  phase_logger_->LogMultipleTaskAssignmentsIOError(
      absl::InternalError(error_message), network_stats_, absl::Now(),
      absl::Now() - absl::Minutes(1));
}

TEST_P(PhaseLoggerImplTest, LogMultipleTaskAssignmentsPayloadIOError) {
  InSequence seq;
  std::string error_message = "Failed to retrieve plan.";
  std::string expected_error_message =
      absl::StrCat("Error during multiple task assignments: code: 13, error: ",
                   error_message);
  EXPECT_CALL(
      mock_event_publisher_,
      PublishMultipleTaskAssignmentsPayloadIOError(expected_error_message));
  EXPECT_CALL(mock_opstats_logger_,
              AddEventWithErrorMessage(
                  OperationalStats::Event::
                      EVENT_KIND_MULTIPLE_TASK_ASSIGNMENTS_ERROR_PAYLOAD_IO,
                  expected_error_message));
  phase_logger_->LogMultipleTaskAssignmentsPayloadIOError(
      absl::InternalError(error_message));
}

TEST_P(PhaseLoggerImplTest, LogMultipleTaskAssignmentsInvalidPayload) {
  InSequence seq;
  EXPECT_CALL(
      mock_log_manager_,
      LogDiag(ProdDiagCode::BACKGROUND_TRAINING_FAILED_CANNOT_PARSE_PLAN));
  std::string error_message = "Unable to parse initial checkpoint.";
  EXPECT_CALL(mock_event_publisher_,
              PublishMultipleTaskAssignmentsInvalidPayload(error_message));
  EXPECT_CALL(
      mock_opstats_logger_,
      AddEventWithErrorMessage(
          OperationalStats::Event::
              EVENT_KIND_MULTIPLE_TASK_ASSIGNMENTS_ERROR_INVALID_PAYLOAD,
          error_message));
  phase_logger_->LogMultipleTaskAssignmentsInvalidPayload(error_message);
}

TEST_P(PhaseLoggerImplTest, LogMultipleTaskAssignmentsClientInterrupted) {
  InSequence seq;
  std::string error_message = "The client is no longer idle";
  std::string expected_error_message =
      absl::StrCat("Error during multiple task assignments: code: 1, error: ",
                   error_message);
  EXPECT_CALL(mock_event_publisher_,
              PublishMultipleTaskAssignmentsClientInterrupted(
                  expected_error_message, network_stats_, _));
  EXPECT_CALL(mock_opstats_logger_,
              AddEventWithErrorMessage(
                  OperationalStats::Event::
                      EVENT_KIND_MULTIPLE_TASK_ASSIGNMENTS_CLIENT_INTERRUPTED,
                  expected_error_message));
  EXPECT_CALL(mock_opstats_logger_, StopLoggingForTheCurrentPhase());
  VerifyCounterLogged(
      HistogramCounters::TRAINING_FL_MULTIPLE_TASK_ASSIGNMENTS_LATENCY, Ge(0));
  VerifyCounterLogged(
      HistogramCounters::TRAINING_FL_MULTIPLE_TASK_ASSIGNMENTS_END_TIME, Ge(0));
  phase_logger_->LogMultipleTaskAssignmentsClientInterrupted(
      absl::CancelledError(error_message), network_stats_, absl::Now(),
      absl::Now() - absl::Minutes(1));
}

TEST_P(PhaseLoggerImplTest, LogMultipleTaskAssignmentsServerAborted) {
  InSequence seq;
  std::string error_message = "The request is aborted by the server";
  std::string expected_error_message =
      absl::StrCat("Error during multiple task assignments: code: 10, error: ",
                   error_message);
  EXPECT_CALL(mock_event_publisher_,
              PublishMultipleTaskAssignmentsServerAborted(
                  expected_error_message, network_stats_, _));
  EXPECT_CALL(mock_opstats_logger_,
              AddEventWithErrorMessage(
                  OperationalStats::Event::
                      EVENT_KIND_MULTIPLE_TASK_ASSIGNMENTS_SERVER_ABORTED,
                  expected_error_message));
  EXPECT_CALL(mock_opstats_logger_, StopLoggingForTheCurrentPhase());
  VerifyCounterLogged(
      HistogramCounters::TRAINING_FL_MULTIPLE_TASK_ASSIGNMENTS_LATENCY, Ge(0));
  VerifyCounterLogged(
      HistogramCounters::TRAINING_FL_MULTIPLE_TASK_ASSIGNMENTS_END_TIME, Ge(0));
  phase_logger_->LogMultipleTaskAssignmentsServerAborted(
      absl::AbortedError(error_message), network_stats_, absl::Now(),
      absl::Now() - absl::Minutes(1));
}

TEST_P(PhaseLoggerImplTest, LogMultipleTaskAssignmentsTurnedAway) {
  InSequence seq;
  EXPECT_CALL(mock_event_publisher_,
              PublishMultipleTaskAssignmentsTurnedAway(network_stats_, _));
  EXPECT_CALL(mock_opstats_logger_,
              AddEvent(OperationalStats::Event::
                           EVENT_KIND_MULTIPLE_TASK_ASSIGNMENTS_REJECTED));
  EXPECT_CALL(mock_opstats_logger_, StopLoggingForTheCurrentPhase());
  VerifyCounterLogged(
      HistogramCounters::TRAINING_FL_MULTIPLE_TASK_ASSIGNMENTS_LATENCY, Ge(0));
  VerifyCounterLogged(
      HistogramCounters::TRAINING_FL_MULTIPLE_TASK_ASSIGNMENTS_END_TIME, Ge(0));
  phase_logger_->LogMultipleTaskAssignmentsTurnedAway(
      network_stats_, absl::Now(), absl::Now() - absl::Minutes(1));
}

TEST_P(PhaseLoggerImplTest, LogMultipleTaskAssignmentsPlanUriReceived) {
  InSequence seq;
  EXPECT_CALL(mock_event_publisher_,
              PublishMultipleTaskAssignmentsPlanUriReceived(network_stats_, _));
  EXPECT_CALL(
      mock_opstats_logger_,
      AddEvent(OperationalStats::Event::
                   EVENT_KIND_MULTIPLE_TASK_ASSIGNMENTS_PLAN_URI_RECEIVED));
  phase_logger_->LogMultipleTaskAssignmentsPlanUriReceived(
      network_stats_, absl::Now() - absl::Minutes(2));
}

TEST_P(PhaseLoggerImplTest, LogMultipleTaskAssignmentsPlanUriPartialReceived) {
  InSequence seq;
  EXPECT_CALL(
      mock_event_publisher_,
      PublishMultipleTaskAssignmentsPlanUriPartialReceived(network_stats_, _));
  EXPECT_CALL(
      mock_opstats_logger_,
      AddEvent(
          OperationalStats::Event::
              EVENT_KIND_MULTIPLE_TASK_ASSIGNMENTS_PLAN_URI_PARTIAL_RECEIVED));
  phase_logger_->LogMultipleTaskAssignmentsPlanUriPartialReceived(
      network_stats_, absl::Now() - absl::Minutes(2));
}

TEST_P(PhaseLoggerImplTest, LogMultipleTaskAssignmentsPartialCompleted) {
  InSequence seq;
  EXPECT_CALL(
      mock_event_publisher_,
      PublishMultipleTaskAssignmentsPartialCompleted(network_stats_, _));
  EXPECT_CALL(
      mock_opstats_logger_,
      AddEvent(OperationalStats::Event::
                   EVENT_KIND_MULTIPLE_TASK_ASSIGNMENTS_PARTIAL_COMPLETED));
  EXPECT_CALL(mock_opstats_logger_, StopLoggingForTheCurrentPhase());
  VerifyCounterLogged(
      HistogramCounters::TRAINING_FL_MULTIPLE_TASK_ASSIGNMENTS_LATENCY, Ge(0));
  VerifyCounterLogged(
      HistogramCounters::TRAINING_FL_MULTIPLE_TASK_ASSIGNMENTS_END_TIME, Ge(0));
  phase_logger_->LogMultipleTaskAssignmentsPartialCompleted(
      network_stats_, absl::Now() - absl::Minutes(2),
      absl::Now() - absl::Minutes(1), absl::Now() - absl::Minutes(4));
}

TEST_P(PhaseLoggerImplTest, LogMultipleTaskAssignmentsCompleted) {
  InSequence seq;
  EXPECT_CALL(mock_event_publisher_,
              PublishMultipleTaskAssignmentsCompleted(network_stats_, _));
  EXPECT_CALL(mock_opstats_logger_,
              AddEvent(OperationalStats::Event::
                           EVENT_KIND_MULTIPLE_TASK_ASSIGNMENTS_COMPLETED));
  EXPECT_CALL(mock_opstats_logger_, StopLoggingForTheCurrentPhase());
  VerifyCounterLogged(
      HistogramCounters::TRAINING_FL_MULTIPLE_TASK_ASSIGNMENTS_LATENCY, Ge(0));
  VerifyCounterLogged(
      HistogramCounters::TRAINING_FL_MULTIPLE_TASK_ASSIGNMENTS_END_TIME, Ge(0));
  phase_logger_->LogMultipleTaskAssignmentsCompleted(
      network_stats_, absl::Now() - absl::Minutes(2),
      absl::Now() - absl::Minutes(1), absl::Now() - absl::Minutes(4));
}

TEST_P(PhaseLoggerImplTest, LogEligibilityEvalComputationErrorNonfatal) {
  absl::Status error = absl::InternalError("oh no!");
  EXPECT_CALL(mock_event_publisher_,
              PublishEligibilityEvalComputationErrorNonfatal(
                  "Error during eligibility eval computation: code: 13, error: "
                  "oh no!"));

  phase_logger_->LogEligibilityEvalComputationErrorNonfatal(error);
}

}  // namespace
}  // namespace client
}  // namespace fcp
