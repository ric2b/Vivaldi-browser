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

#include <memory>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "fcp/base/clock.h"

namespace fcp {
namespace {

using ::testing::ElementsAre;
using ::testing::Test;

class RealtimeClockTest : public Test {
 public:
  RealtimeClockTest() : start_(Clock::RealClock()->Now()) {}

  void OnWakeUp(int waiter_id) {
    absl::MutexLock lock(&mu_);
    waiter_ids_.push_back(waiter_id);
    waiter_intervals_.push_back(Clock::RealClock()->Now() - start_);
  }

 protected:
  absl::Time start_;
  absl::Mutex mu_;
  std::vector<int> waiter_ids_ ABSL_GUARDED_BY(&mu_);
  std::vector<absl::Duration> waiter_intervals_ ABSL_GUARDED_BY(&mu_);
};

// Simple callback waiter that records current waiter ID with the test.
class TestWaiter : public Clock::Waiter {
 public:
  explicit TestWaiter(int id, RealtimeClockTest* test) : id_(id), test_(test) {}

  void WakeUp() override { test_->OnWakeUp(id_); }

 private:
  int id_;
  RealtimeClockTest* test_;
};

TEST_F(RealtimeClockTest, MultipleTimerWakeUp) {
  // Add 4 timers at various deadlines, the last one in the past.
  Clock::RealClock()->WakeupWithDeadline(start_ + absl::Milliseconds(200),
                                         std::make_shared<TestWaiter>(1, this));
  Clock::RealClock()->WakeupWithDeadline(start_ + absl::Milliseconds(100),
                                         std::make_shared<TestWaiter>(2, this));
  Clock::RealClock()->WakeupWithDeadline(start_ + absl::Milliseconds(101),
                                         std::make_shared<TestWaiter>(3, this));
  Clock::RealClock()->WakeupWithDeadline(start_ - absl::Milliseconds(1),
                                         std::make_shared<TestWaiter>(4, this));

  // End the test when all 3 timers have been triggered.
  auto test_done = [this]() {
    mu_.AssertHeld();
    return waiter_ids_.size() == 4;
  };

  absl::MutexLock lock(&mu_);
  mu_.Await(absl::Condition(&test_done));

  // Verify the results
  EXPECT_THAT(waiter_ids_, ElementsAre(4, 2, 3, 1));
  EXPECT_GE(waiter_intervals_[0], absl::ZeroDuration());
  EXPECT_GE(waiter_intervals_[1], absl::Milliseconds(100));
  EXPECT_GE(waiter_intervals_[2], absl::Milliseconds(101));
  EXPECT_GE(waiter_intervals_[3], absl::Milliseconds(200));
}

}  // namespace
}  // namespace fcp
