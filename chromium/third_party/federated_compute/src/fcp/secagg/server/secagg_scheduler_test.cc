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

#include "fcp/secagg/server/secagg_scheduler.h"

#include <atomic>
#include <functional>
#include <memory>
#include <utility>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/synchronization/notification.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "fcp/base/monitoring.h"
#include "fcp/base/scheduler.h"
#include "fcp/base/simulated_clock.h"

namespace fcp {
namespace secagg {
namespace {

using ::testing::_;
using ::testing::Eq;
using ::testing::IsFalse;
using ::testing::Lt;
using ::testing::StrictMock;
using ::testing::Test;

class MockScheduler : public Scheduler {
 public:
  MOCK_METHOD(void, Schedule, (std::function<void()>), (override));
  MOCK_METHOD(void, WaitUntilIdle, ());
};

// Wrap int in a struct to keep Clang-tidy happy.
struct Integer {
  Integer() : value(0) {}
  explicit Integer(int v) : value(v) {}
  int value;
};

std::vector<std::function<std::unique_ptr<Integer>()>> IntGenerators(int n) {
  std::vector<std::function<std::unique_ptr<Integer>()>> generators;
  for (int i = 1; i <= n; ++i) {
    generators.emplace_back([i]() { return std::make_unique<Integer>(i); });
  }
  return generators;
}

constexpr auto multiply_accumulator = [](const Integer& l, const Integer& r) {
  return std::make_unique<Integer>(l.value * r.value);
};
constexpr auto call_fn = [](const std::function<void()>& f) { f(); };

TEST(SecAggSchedulerTest, ScheduleCallback) {
  StrictMock<MockScheduler> parallel_scheduler;
  StrictMock<MockScheduler> sequential_scheduler;

  EXPECT_CALL(parallel_scheduler, Schedule(_)).Times(0);
  EXPECT_CALL(sequential_scheduler, Schedule(_)).WillOnce(call_fn);

  SecAggScheduler runner(&parallel_scheduler, &sequential_scheduler);

  int r = 0;
  runner.ScheduleCallback([&r]() { r = 5; });
  EXPECT_THAT(r, Eq(5));
}

TEST(SecAggSchedulerTest, SingleCall) {
  StrictMock<MockScheduler> parallel_scheduler;
  StrictMock<MockScheduler> sequential_scheduler;

  EXPECT_CALL(parallel_scheduler, Schedule(_)).Times(6).WillRepeatedly(call_fn);
  EXPECT_CALL(sequential_scheduler, Schedule(_))
      .Times(7)
      .WillRepeatedly(call_fn);

  // Technically unsafe, but we know the pointers will be valid as long as
  // runner is alive.
  SecAggScheduler runner(&parallel_scheduler, &sequential_scheduler);

  std::vector<std::function<std::unique_ptr<Integer>()>> generators =
      IntGenerators(6);

  Integer result;
  auto accumulator = runner.CreateAccumulator<Integer>(
      std::make_unique<Integer>(1), multiply_accumulator);
  for (const auto& generator : generators) {
    accumulator->Schedule(generator);
  }
  accumulator->SetAsyncObserver(
      [&]() { result = *(accumulator->GetResultAndCancel()); });
  EXPECT_THAT(result.value, Eq(720));  // 6! = 720
}

TEST(SecAggSchedulerTest, SingleCallWithDelay) {
  StrictMock<MockScheduler> parallel_scheduler;
  StrictMock<MockScheduler> sequential_scheduler;
  SimulatedClock clock;

  EXPECT_CALL(parallel_scheduler, Schedule(_)).Times(6).WillRepeatedly(call_fn);
  EXPECT_CALL(sequential_scheduler, Schedule(_))
      .Times(6)
      .WillRepeatedly(call_fn);

  SecAggScheduler runner(&parallel_scheduler, &sequential_scheduler, &clock);

  std::vector<std::function<std::unique_ptr<Integer>()>> generators =
      IntGenerators(6);

  Integer result;
  auto accumulator = runner.CreateAccumulator<Integer>(
      std::make_unique<Integer>(1), multiply_accumulator);
  for (const auto& generator : generators) {
    accumulator->Schedule(generator, absl::Seconds(5));
  }
  accumulator->SetAsyncObserver(
      [&]() { result = *(accumulator->GetResultAndCancel()); });

  // Generators are still delayed.
  EXPECT_THAT(result.value, Eq(0));

  // Advance time by one second.
  clock.AdvanceTime(absl::Seconds(1));
  // Generators are still delayed.
  EXPECT_THAT(result.value, Eq(0));

  // Advance time by another 4 seconds.
  clock.AdvanceTime(absl::Seconds(4));
  EXPECT_THAT(result.value, Eq(720));  // 6! = 720
}

TEST(SecAggSchedulerTest, TwoCalls) {
  StrictMock<MockScheduler> parallel_scheduler;
  StrictMock<MockScheduler> sequential_scheduler;

  EXPECT_CALL(parallel_scheduler, Schedule(_)).WillRepeatedly(call_fn);
  EXPECT_CALL(sequential_scheduler, Schedule(_)).WillRepeatedly(call_fn);

  // Technically unsafe, but we know the pointers will be valid as long as
  // runner is alive.
  SecAggScheduler runner(&parallel_scheduler, &sequential_scheduler);

  // First call
  std::vector<std::function<std::unique_ptr<Integer>()>> generators =
      IntGenerators(6);

  Integer result;
  auto accumulator = runner.CreateAccumulator<Integer>(
      std::make_unique<Integer>(1), multiply_accumulator);
  for (const auto& generator : generators) {
    accumulator->Schedule(generator);
  }
  accumulator->SetAsyncObserver(
      [&]() { result = *(accumulator->GetResultAndCancel()); });

  EXPECT_THAT(result.value, Eq(720));  // 6! = 720

  // Second call
  std::vector<std::function<std::unique_ptr<Integer>()>> generators2 =
      IntGenerators(4);
  auto accumulator2 = runner.CreateAccumulator<Integer>(
      std::make_unique<Integer>(1), multiply_accumulator);

  for (const auto& generator : generators2) {
    accumulator2->Schedule(generator);
  }
  accumulator2->SetAsyncObserver(
      [&]() { result = *(accumulator2->GetResultAndCancel()); });
  EXPECT_THAT(result.value, Eq(24));  // 4! = 24
}

TEST(SecAggSchedulerAbortTest, Abort) {
  auto parallel_scheduler = fcp::CreateThreadPoolScheduler(4);
  auto sequential_scheduler = fcp::CreateThreadPoolScheduler(1);

  absl::Notification signal_abort;
  std::atomic<int> callback_counter = 0;

  std::vector<std::function<std::unique_ptr<Integer>()>> generators;
  for (int i = 1; i <= 100; ++i) {
    generators.emplace_back([&, i]() {
      callback_counter++;
      // Signal abort when running 10th parallel task
      if (i == 10) {
        signal_abort.Notify();
      }
      absl::SleepFor(absl::Milliseconds(1));
      return std::make_unique<Integer>(i);
    });
  }

  auto accumulator_func = [&](const Integer& l, const Integer& r) {
    callback_counter++;
    return std::make_unique<Integer>(l.value * r.value);
  };

  SecAggScheduler runner(parallel_scheduler.get(), sequential_scheduler.get());
  bool final_callback_called = false;
  auto accumulator = runner.CreateAccumulator<Integer>(
      std::make_unique<Integer>(1), accumulator_func);
  for (const auto& generator : generators) {
    accumulator->Schedule(generator);
  }
  accumulator->SetAsyncObserver([&]() { final_callback_called = true; });

  signal_abort.WaitForNotification();
  accumulator->Cancel();

  int count_after_abort = callback_counter.load();
  FCP_LOG(INFO) << "count_after_abort = " << count_after_abort;

  // Wait for all scheduled tasks to finish
  runner.WaitUntilIdle();

  // The final number of callbacks should not change since returning from
  // Abort.
  int final_count = callback_counter.load();
  EXPECT_THAT(final_count, Eq(count_after_abort));
  EXPECT_THAT(final_count, Lt(generators.size()));
  EXPECT_THAT(final_callback_called, IsFalse());
}

// Tests that three batches of async work result in three calls to the callback,
// which can be overriden in between calls.
TEST(SecAggSchedulerTest, ThreeCallbackCalls) {
  auto parallel_scheduler = fcp::CreateThreadPoolScheduler(4);
  auto sequential_scheduler = fcp::CreateThreadPoolScheduler(1);

  SecAggScheduler runner(parallel_scheduler.get(), sequential_scheduler.get());

  std::vector<std::function<std::unique_ptr<Integer>()>> generators =
      IntGenerators(3);

  auto accumulator = runner.CreateAccumulator<Integer>(
      std::make_unique<Integer>(1), multiply_accumulator);
  for (const auto& generator : generators) {
    accumulator->Schedule(generator);
  }
  int callback_counter = 0;
  accumulator->SetAsyncObserver([&]() { callback_counter++; });
  runner.WaitUntilIdle();
  EXPECT_THAT(callback_counter, Eq(1));
  for (const auto& generator : generators) {
    accumulator->Schedule(generator);
  }
  runner.WaitUntilIdle();
  // The callback was not re-scheduled, so the second call to Schedule didn't
  // trigger it. This results in unobserved work.
  EXPECT_THAT(callback_counter, Eq(1));
  bool has_work = accumulator->SetAsyncObserver([&]() { callback_counter++; });
  runner.WaitUntilIdle();
  EXPECT_TRUE(has_work);
  EXPECT_THAT(callback_counter, Eq(2));
  // The accumulator should be idle and without unobserved work at this point.
  has_work = accumulator->SetAsyncObserver([&]() { callback_counter++; });
  EXPECT_FALSE(has_work);
  Integer result;
  for (const auto& generator : generators) {
    accumulator->Schedule(generator);
  }
  accumulator->SetAsyncObserver(
      [&]() { result = *(accumulator->GetResultAndCancel()); });
  runner.WaitUntilIdle();
  // The last call to SetAsyncObserver overwrittes the previous callback.
  EXPECT_THAT(callback_counter, Eq(2));
  EXPECT_THAT(result.value, Eq(216));  // 6^3 = 216
}

}  // namespace
}  // namespace secagg
}  // namespace fcp
