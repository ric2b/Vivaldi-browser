/*
 * Copyright 2017 Google LLC
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

#include "fcp/base/monitoring.h"

#include <stdio.h>

#include <array>
#include <memory>
#include <string>
#include <utility>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/base/log_severity.h"
#include "absl/strings/str_format.h"
#include "fcp/base/base_name.h"

namespace fcp {
namespace {

using ::testing::MatchesRegex;
using ::testing::Not;

MATCHER(IsOk, "") { return arg.ok(); }

MATCHER_P(IsOkAndHolds, m, "") {
  return testing::ExplainMatchResult(IsOk(), arg, result_listener) &&
         testing::ExplainMatchResult(m, arg.value(), result_listener);
}

class MonitoringTest : public ::testing::TestWithParam<bool> {
 public:
  void SetUp() override {
    // The first log message will make Absl print a warning about how all logs
    // are routed to stderr until absl::InitializeLog is called. We do want the
    // logs to go to stderr for this test, but we do not want this warning
    // message to occur in the captured output of any of the tests below. So we
    // log an initial message here to trigger the warning early, before any
    // tests actually run.
    FCP_LOG(INFO) << "Test log message. You can ignore this.";
  }
};

TEST_F(MonitoringTest, LogInfo) {
  testing::internal::CaptureStderr();
  FCP_LOG(INFO) << "info log of something happening";
  std::string output = testing::internal::GetCapturedStderr();
  ASSERT_THAT(output, MatchesRegex("I.*info log of something happening\n"));
}

TEST_F(MonitoringTest, LogWarning) {
  testing::internal::CaptureStderr();
  FCP_LOG(WARNING) << "warning log of something happening";
  std::string output = testing::internal::GetCapturedStderr();
  ASSERT_THAT(output, MatchesRegex("W.*warning log of something happening\n"));
}

TEST_F(MonitoringTest, LogError) {
  testing::internal::CaptureStderr();
  FCP_LOG(ERROR) << "error log of something happening";
  std::string output = testing::internal::GetCapturedStderr();
  ASSERT_THAT(output, MatchesRegex("E.*error log of something happening\n"));
}

TEST_F(MonitoringTest, LogFatal) {
  ASSERT_DEATH({ FCP_LOG(FATAL) << "fatal log"; }, "fatal log");
}

TEST_F(MonitoringTest, LogIfTrue) {
  testing::internal::CaptureStderr();
  FCP_LOG_IF(INFO, true) << "some log";
  std::string output = testing::internal::GetCapturedStderr();
  ASSERT_THAT(output, MatchesRegex("I.*some log\n"));
}

TEST_F(MonitoringTest, LogIfFalse) {
  testing::internal::CaptureStderr();
  FCP_LOG_IF(INFO, false) << "some log";
  std::string output = testing::internal::GetCapturedStderr();
  ASSERT_EQ(output, "");
}

TEST_F(MonitoringTest, CheckSucceeds) { FCP_CHECK(1 < 2); }

TEST_F(MonitoringTest, CheckFails) {
  ASSERT_DEATH({ FCP_CHECK(1 < 0); }, "Check failed: 1 < 0.");
}

TEST_F(MonitoringTest, StatusBuilder) {
  ASSERT_FALSE(FCP_STATUS(ABORTED).ok());
  ASSERT_EQ(FCP_STATUS(ABORTED).code(), ABORTED);
}

TEST_F(MonitoringTest, FcpReturnIfError) {
  ASSERT_THAT(
      []() -> StatusOr<int> {
        Status fail_status = FCP_STATUS(ABORTED);
        FCP_RETURN_IF_ERROR(fail_status);
        return 0;
      }(),
      Not(IsOk()));
  ASSERT_THAT(
      []() -> StatusOr<int> {
        FCP_RETURN_IF_ERROR(Status());
        return 0;
      }(),
      IsOkAndHolds(0));

  ASSERT_THAT(
      []() -> StatusOr<int> {
        StatusOr<int> fail_statusor = FCP_STATUS(ABORTED);
        FCP_RETURN_IF_ERROR(fail_statusor);
        return 0;
      }(),
      Not(IsOk()));
  ASSERT_THAT(
      []() -> StatusOr<int> {
        FCP_RETURN_IF_ERROR(StatusOr<int>(0));
        return 0;
      }(),
      IsOkAndHolds(0));
}

}  // namespace
}  // namespace fcp
