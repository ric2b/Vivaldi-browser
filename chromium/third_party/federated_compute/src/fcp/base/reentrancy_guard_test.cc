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

#include "fcp/base/reentrancy_guard.h"

#include <atomic>

#include "gtest/gtest.h"
#include "absl/synchronization/notification.h"
#include "fcp/base/monitoring.h"
#include "fcp/base/scheduler.h"
#include "fcp/testing/testing.h"

namespace fcp {
namespace {

class ReentrancyGuardTest : public testing::Test {
 protected:
  Status SimpleMethod() {
    ReentrancyGuard guard;
    return guard.Check(&in_use_);
  }

  Status ReentrantMethod() {
    ReentrancyGuard guard;
    FCP_RETURN_IF_ERROR((guard.Check(&in_use_)));
    return ReentrantMethod();
  }

  Status LongRunningMethod(absl::Notification* method_entered,
                           absl::Notification* resume) {
    ReentrancyGuard guard;
    FCP_RETURN_IF_ERROR((guard.Check(&in_use_)));
    method_entered->Notify();
    resume->WaitForNotification();
    return FCP_STATUS(OK);
  }

 private:
  std::atomic<bool> in_use_ = false;
};

TEST_F(ReentrancyGuardTest, SequentialCallsSucceed) {
  ASSERT_THAT(SimpleMethod(), IsOk());
  ASSERT_THAT(SimpleMethod(), IsOk());
}

TEST_F(ReentrancyGuardTest, ReentrantCallsFail) {
  ASSERT_THAT(ReentrantMethod(), IsCode(FAILED_PRECONDITION));
}

TEST_F(ReentrancyGuardTest, ConcurrentCallsFail) {
  absl::Notification long_running_method_entered;
  absl::Notification resume;

  auto pool = fcp::CreateThreadPoolScheduler(1);
  pool->Schedule([&] {
    ASSERT_THAT(LongRunningMethod(&long_running_method_entered, &resume),
                IsOk());
  });

  // This signals that LongRunningMethod() has been entered and waits there
  // to be resumed.
  long_running_method_entered.WaitForNotification();

  // Make a concurrent call, which is expected to fail.
  ASSERT_THAT(SimpleMethod(), IsCode(FAILED_PRECONDITION));

  // Resume LongRunningMethod() and wait for the thread to finish.
  resume.Notify();
  pool->WaitUntilIdle();
}

}  // namespace
}  // namespace fcp
