// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.


#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "fcp/client/test_helpers.h"

namespace fcp {
namespace client {
namespace {

using ::testing::Return;
using ::testing::StrictMock;

TEST(SimpleTaskEnvironmentTest, TestShouldAbort) {
  StrictMock<MockSimpleTaskEnvironment> mock_task_env;
  EXPECT_CALL(mock_task_env, TrainingConditionsSatisfied())
      .WillOnce(Return(false));
  bool result = mock_task_env.ShouldAbort(
      /*current_time=*/absl::Now(),
      /*condition_polling_period=*/absl::ZeroDuration());
  EXPECT_TRUE(result);
}

// Assert that with a zero condition_polling_period, no throttling in
// ShouldAbort takes place, by emulating two calls at the same time.
// Both calls should return the mock's value.
TEST(SimpleTaskEnvironmentTest, TestShouldAbortNoThrottling) {
  StrictMock<MockSimpleTaskEnvironment> mock_task_env;
  absl::Time now = absl::Now();
  EXPECT_CALL(mock_task_env, TrainingConditionsSatisfied())
      .WillRepeatedly(Return(false));
  bool result = mock_task_env.ShouldAbort(
      /*current_time=*/now,
      /*condition_polling_period=*/absl::ZeroDuration());
  EXPECT_TRUE(result);
  result = mock_task_env.ShouldAbort(
      /*current_time=*/now,
      /*condition_polling_period=*/absl::ZeroDuration());
  EXPECT_TRUE(result);
}

// Verify ShouldAbort throttling for non-zero polling periods.
TEST(SimpleTaskEnvironmentTest, TestShouldAbortThrottling) {
  StrictMock<MockSimpleTaskEnvironment> mock_task_env;
  EXPECT_CALL(mock_task_env, TrainingConditionsSatisfied())
      .WillRepeatedly(Return(false));
  absl::Time now = absl::Now();
  // First call should be non-throttled (since it assumes last call happened at
  // UnixEpoch. Second call after 1s will be throttled because polling period is
  // 1.5s; third call (after 2s) will be non-throttled again.
  bool result = mock_task_env.ShouldAbort(
      /*current_time=*/now,
      /*condition_polling_period=*/absl::Milliseconds(1500));
  EXPECT_TRUE(result);
  result = mock_task_env.ShouldAbort(
      /*current_time=*/now + absl::Seconds(1),
      /*condition_polling_period=*/absl::Milliseconds(1500));
  EXPECT_FALSE(result);
  result = mock_task_env.ShouldAbort(
      /*current_time=*/now + absl::Seconds(2),
      /*condition_polling_period=*/absl::Milliseconds(1500));
  EXPECT_TRUE(result);
}

}  // anonymous namespace
}  // namespace client
}  // namespace fcp
