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
#include "fcp/client/interruptible_runner.h"

#include <functional>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/synchronization/blocking_counter.h"
#include "absl/time/time.h"
#include "fcp/base/monitoring.h"
#include "fcp/client/diag_codes.pb.h"
#include "fcp/client/test_helpers.h"
#include "fcp/testing/testing.h"

namespace fcp {
namespace client {
namespace {

using ::fcp::client::ProdDiagCode::BACKGROUND_TRAINING_INTERRUPT_TF_EXECUTION;
using ::fcp::client::ProdDiagCode::
    BACKGROUND_TRAINING_INTERRUPT_TF_EXECUTION_TIMED_OUT;
using ::fcp::client::ProdDiagCode::
    BACKGROUND_TRAINING_INTERRUPT_TF_EXTENDED_EXECUTION_COMPLETED;
using ::fcp::client::ProdDiagCode::
    BACKGROUND_TRAINING_INTERRUPT_TF_EXTENDED_EXECUTION_TIMED_OUT;
using ::testing::StrictMock;

static InterruptibleRunner::DiagnosticsConfig getDiagnosticsConfig() {
  return InterruptibleRunner::DiagnosticsConfig{
      .interrupted = BACKGROUND_TRAINING_INTERRUPT_TF_EXECUTION,
      .interrupt_timeout = BACKGROUND_TRAINING_INTERRUPT_TF_EXECUTION_TIMED_OUT,
      .interrupted_extended =
          BACKGROUND_TRAINING_INTERRUPT_TF_EXTENDED_EXECUTION_COMPLETED,
      .interrupt_timeout_extended =
          BACKGROUND_TRAINING_INTERRUPT_TF_EXTENDED_EXECUTION_TIMED_OUT};
}

// Tests the case where runnable finishes before the future times out (and we'd
// call should_abort).
TEST(InterruptibleRunnerTest, TestNormalNoAbortCheck) {
  int should_abort_calls = 0;
  int abort_function_calls = 0;
  std::function<bool()> should_abort = [&should_abort_calls]() {
    should_abort_calls++;
    return false;
  };
  std::function<void()> abort_function = [&abort_function_calls]() {
    abort_function_calls++;
  };

  InterruptibleRunner interruptibleRunner(
      /*log_manager=*/nullptr, should_abort,
      InterruptibleRunner::TimingConfig{
          .polling_period = absl::InfiniteDuration(),
          .graceful_shutdown_period = absl::InfiniteDuration(),
          .extended_shutdown_period = absl::InfiniteDuration()},
      getDiagnosticsConfig());
  absl::Status status = interruptibleRunner.Run(
      []() { return absl::OkStatus(); }, abort_function);
  EXPECT_THAT(status, IsCode(OK));
  EXPECT_EQ(should_abort_calls, 1);
  EXPECT_EQ(abort_function_calls, 0);

  // Test that the Status returned by the runnable is returned as is.
  status = interruptibleRunner.Run([]() { return absl::DataLossError(""); },
                                   abort_function);
  EXPECT_THAT(status, IsCode(DATA_LOSS));
}

// Tests the case where should_abort prevents us from even kicking off the run.
TEST(InterruptibleRunnerTest, TestNormalAbortBeforeRun) {
  int should_abort_calls = 0;
  int abort_function_calls = 0;
  int runnable_calls = 0;
  std::function<bool()> should_abort = [&should_abort_calls]() {
    should_abort_calls++;
    return true;
  };
  std::function<void()> abort_function = [&abort_function_calls]() {
    abort_function_calls++;
  };

  InterruptibleRunner interruptibleRunner(
      /*log_manager=*/nullptr, should_abort,
      InterruptibleRunner::TimingConfig{
          .polling_period = absl::InfiniteDuration(),
          .graceful_shutdown_period = absl::InfiniteDuration(),
          .extended_shutdown_period = absl::InfiniteDuration()},
      getDiagnosticsConfig());
  absl::Status status = interruptibleRunner.Run(
      [&runnable_calls]() {
        runnable_calls++;
        return absl::OkStatus();
      },
      abort_function);
  EXPECT_THAT(status, IsCode(CANCELLED));
  EXPECT_EQ(abort_function_calls, 0);
  EXPECT_EQ(runnable_calls, 0);
}

// Tests the case where the future wait times out once, we call should_abort,
// which says to continue, and then the future returns.
TEST(InterruptibleRunnerTest, TestNormalWithAbortCheckButNoAbort) {
  int should_abort_calls = 0;
  int abort_function_calls = 0;
  absl::BlockingCounter counter_should_abort(1);
  absl::BlockingCounter counter_did_abort(1);
  std::function<bool()> should_abort =
      [&should_abort_calls, &counter_should_abort, &counter_did_abort]() {
        should_abort_calls++;
        if (should_abort_calls == 2) {
          counter_should_abort.DecrementCount();
          counter_did_abort.Wait();
        }
        return false;
      };
  std::function<void()> abort_function = [&abort_function_calls]() {
    abort_function_calls++;
  };

  InterruptibleRunner interruptibleRunner(
      nullptr, should_abort,
      InterruptibleRunner::TimingConfig{
          .polling_period = absl::ZeroDuration(),
          .graceful_shutdown_period = absl::InfiniteDuration(),
          .extended_shutdown_period = absl::InfiniteDuration()},
      getDiagnosticsConfig());
  absl::Status status = interruptibleRunner.Run(
      [&counter_should_abort, &counter_did_abort]() {
        // Block until should_abort has been called.
        counter_should_abort.Wait();
        // Tell should_abort to return false.
        counter_did_abort.DecrementCount();
        return absl::OkStatus();
      },
      abort_function);
  EXPECT_THAT(status, IsCode(OK));
  EXPECT_GE(should_abort_calls, 2);
  EXPECT_EQ(abort_function_calls, 0);

  status = interruptibleRunner.Run([]() { return absl::DataLossError(""); },
                                   abort_function);
  EXPECT_THAT(status, IsCode(DATA_LOSS));
}

// Tests the case where the runnable gets aborted and behaves nicely (aborts
// within the grace period).
TEST(InterruptibleRunnerTest, TestAbortInGracePeriod) {
  StrictMock<MockLogManager> log_manager;
  int should_abort_calls = 0;
  int abort_function_calls = 0;
  absl::BlockingCounter counter_should_abort(1);
  absl::BlockingCounter counter_did_abort(1);

  std::function<bool()> should_abort = [&should_abort_calls]() {
    should_abort_calls++;
    return should_abort_calls >= 2;
  };
  std::function<void()> abort_function =
      [&abort_function_calls, &counter_should_abort, &counter_did_abort]() {
        abort_function_calls++;
        // Signal runnable to abort.
        counter_should_abort.DecrementCount();
        // Wait for runnable to have aborted.
        counter_did_abort.Wait();
      };

  InterruptibleRunner interruptibleRunner(
      &log_manager, should_abort,
      InterruptibleRunner::TimingConfig{
          .polling_period = absl::ZeroDuration(),
          .graceful_shutdown_period = absl::InfiniteDuration(),
          .extended_shutdown_period = absl::InfiniteDuration()},
      getDiagnosticsConfig());
  // Tests that abort works.
  EXPECT_CALL(log_manager, LogDiag(BACKGROUND_TRAINING_INTERRUPT_TF_EXECUTION))
      .Times(testing::Exactly(1));
  absl::Status status = interruptibleRunner.Run(
      [&counter_should_abort, &counter_did_abort]() {
        counter_should_abort.Wait();
        counter_did_abort.DecrementCount();
        return absl::OkStatus();
      },
      abort_function);
  EXPECT_THAT(status, IsCode(CANCELLED));
  EXPECT_EQ(should_abort_calls, 2);
  EXPECT_EQ(abort_function_calls, 1);
}

// Tests the case where abort does not happen within the grace period.
// This is achieved by only letting the runnable finish once the grace period
// wait fails and a timeout diag code is logged, by taking an action on the
// LogManager mock.
TEST(InterruptibleRunnerTest, TestAbortInExtendedGracePeriod) {
  StrictMock<MockLogManager> log_manager;
  int should_abort_calls = 0;
  int abort_function_calls = 0;

  absl::BlockingCounter counter_should_abort(1);
  absl::BlockingCounter counter_did_abort(1);

  std::function<bool()> should_abort = [&should_abort_calls]() {
    should_abort_calls++;
    return should_abort_calls >= 2;
  };
  std::function<void()> abort_function = [&abort_function_calls]() {
    abort_function_calls++;
  };

  InterruptibleRunner interruptibleRunner(
      &log_manager, should_abort,
      InterruptibleRunner::TimingConfig{
          .polling_period = absl::ZeroDuration(),
          .graceful_shutdown_period = absl::ZeroDuration(),
          .extended_shutdown_period = absl::InfiniteDuration()},
      getDiagnosticsConfig());
  EXPECT_CALL(log_manager,
              LogDiag(BACKGROUND_TRAINING_INTERRUPT_TF_EXECUTION_TIMED_OUT))
      .WillOnce(
          [&counter_should_abort, &counter_did_abort](ProdDiagCode ignored) {
            counter_should_abort.DecrementCount();
            counter_did_abort.Wait();
            return absl::OkStatus();
          });
  EXPECT_CALL(
      log_manager,
      LogDiag(BACKGROUND_TRAINING_INTERRUPT_TF_EXTENDED_EXECUTION_COMPLETED))
      .Times(testing::Exactly(1));
  absl::Status status = interruptibleRunner.Run(
      [&counter_should_abort, &counter_did_abort]() {
        counter_should_abort.Wait();
        counter_did_abort.DecrementCount();
        return absl::OkStatus();
      },
      abort_function);

  EXPECT_THAT(status, IsCode(CANCELLED));
  EXPECT_EQ(should_abort_calls, 2);
  EXPECT_EQ(abort_function_calls, 1);
}

}  // namespace
}  // namespace client
}  // namespace fcp
