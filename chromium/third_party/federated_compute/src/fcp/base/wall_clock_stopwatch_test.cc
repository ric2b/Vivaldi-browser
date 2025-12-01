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

#include "fcp/base/wall_clock_stopwatch.h"

#include <memory>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/time/time.h"
#include "fcp/base/clock.h"
#include "fcp/base/scheduler.h"

namespace fcp {

using ::testing::AllOf;
using ::testing::Eq;
using ::testing::Ge;
using ::testing::Lt;

TEST(WallClockStopwatchTest, NoopHandle) {
  // These noop handles should not crash (or do anything).
  auto stopwatch = WallClockStopwatch::CreateNoop();
  {
    auto started_stopwatch1 = stopwatch->Start();
    auto started_stopwatch2 = stopwatch->Start();
  }
  EXPECT_THAT(stopwatch->GetTotalDuration(), Eq(absl::ZeroDuration()));
}

TEST(WallClockStopwatchTest, ShouldBeInitializedToZero) {
  auto stopwatch = WallClockStopwatch::Create();
  EXPECT_THAT(stopwatch->GetTotalDuration(), Eq(absl::ZeroDuration()));
}

TEST(WallClockStopwatchTest, SingleThreadSingleStart) {
  auto stopwatch = WallClockStopwatch::Create();

  {
    auto started_stopwatch = stopwatch->Start();
    absl::SleepFor(absl::Milliseconds(100));
  }

  EXPECT_THAT(stopwatch->GetTotalDuration(), Ge(absl::Milliseconds(100)));
}

TEST(WallClockStopwatchTest, SingleThreadMultipleSequentialStartStop) {
  auto stopwatch = WallClockStopwatch::Create();

  {
    auto started_stopwatch = stopwatch->Start();
    absl::SleepFor(absl::Milliseconds(100));
  }

  EXPECT_THAT(stopwatch->GetTotalDuration(), Ge(absl::Milliseconds(100)));

  absl::SleepFor(absl::Milliseconds(100));
  // The SleepFor should not be reflect in the measurement, since the stopwatch
  // was stopped.
  EXPECT_THAT(stopwatch->GetTotalDuration(), Ge(absl::Milliseconds(100)));

  {
    auto started_stopwatch = stopwatch->Start();
    absl::SleepFor(absl::Milliseconds(100));
  }
  EXPECT_THAT(stopwatch->GetTotalDuration(), Ge(absl::Milliseconds(200)));
}

TEST(WallClockStopwatchTest, ShouldReflectOngoingMeasurement) {
  auto stopwatch = WallClockStopwatch::Create();

  {
    auto started_stopwatch = stopwatch->Start();
    absl::SleepFor(absl::Milliseconds(100));
    EXPECT_THAT(stopwatch->GetTotalDuration(), Ge(absl::Milliseconds(100)));
    absl::SleepFor(absl::Milliseconds(100));
  }

  EXPECT_THAT(stopwatch->GetTotalDuration(), Ge(absl::Milliseconds(200)));
}

TEST(WallClockStopwatchTest, SingleThreadMultipleConcurrentStart) {
  auto stopwatch = WallClockStopwatch::Create();

  {
    auto started_stopwatch1 = stopwatch->Start();
    absl::SleepFor(absl::Milliseconds(100));
    EXPECT_THAT(stopwatch->GetTotalDuration(), Ge(absl::Milliseconds(100)));
    {
      auto started_stopwatch2 = stopwatch->Start();
      absl::SleepFor(absl::Milliseconds(100));
      EXPECT_THAT(stopwatch->GetTotalDuration(), Ge(absl::Milliseconds(200)));
      {
        auto started_stopwatch3 = stopwatch->Start();
        absl::SleepFor(absl::Milliseconds(100));
      }
    }
  }
  EXPECT_THAT(stopwatch->GetTotalDuration(), Ge(absl::Milliseconds(300)));
}

/** Tests that the stopwatch truly measures wall clock time, and not the
 * cumulative (but concurrent) time spent in each separate thread. */
TEST(WallClockStopwatchTest, ThreeThreadsThreeTasks) {
  auto stopwatch = WallClockStopwatch::Create();
  std::unique_ptr<Scheduler> scheduler =
      CreateThreadPoolScheduler(/*thread_count=*/3);

  scheduler->Schedule([&stopwatch]() {
    auto started_stopwatch = stopwatch->Start();
    absl::SleepFor(absl::Milliseconds(100));
    EXPECT_THAT(stopwatch->GetTotalDuration(), Ge(absl::Milliseconds(100)));
  });
  scheduler->Schedule([&stopwatch]() {
    auto started_stopwatch = stopwatch->Start();
    absl::SleepFor(absl::Milliseconds(100));
    EXPECT_THAT(stopwatch->GetTotalDuration(), Ge(absl::Milliseconds(100)));
  });
  scheduler->Schedule([&stopwatch]() {
    auto started_stopwatch = stopwatch->Start();
    absl::SleepFor(absl::Milliseconds(100));
    EXPECT_THAT(stopwatch->GetTotalDuration(), Ge(absl::Milliseconds(100)));
  });
  scheduler->WaitUntilIdle();
  // The stopwatch should only have measured ~100ms of wall clock time, since
  // the three threads will have run concurrently (we use a margin of 50 extra
  // ms since these can be quite slow when run with ASAN/TSAN).
  EXPECT_THAT(stopwatch->GetTotalDuration(),
              AllOf(Ge(absl::Milliseconds(100)), Lt(absl::Milliseconds(150))));
}

/** Tests that the stopwatch truly measures wall clock time, but this time in a
 * scenario where there are only 2 threads so the third measurement *will*
 * happen sequentially. */
TEST(WallClockStopwatchTest, TwoThreadsThreeTasks) {
  auto stopwatch = WallClockStopwatch::Create();
  std::unique_ptr<Scheduler> scheduler =
      CreateThreadPoolScheduler(/*thread_count=*/2);

  scheduler->Schedule([&stopwatch]() {
    auto started_stopwatch = stopwatch->Start();
    absl::SleepFor(absl::Milliseconds(100));
    EXPECT_THAT(stopwatch->GetTotalDuration(), Ge(absl::Milliseconds(100)));
  });
  scheduler->Schedule([&stopwatch]() {
    auto started_stopwatch = stopwatch->Start();
    absl::SleepFor(absl::Milliseconds(100));
    EXPECT_THAT(stopwatch->GetTotalDuration(), Ge(absl::Milliseconds(100)));
  });
  scheduler->Schedule([&stopwatch]() {
    auto started_stopwatch = stopwatch->Start();
    absl::SleepFor(absl::Milliseconds(100));
    EXPECT_THAT(stopwatch->GetTotalDuration(), Ge(absl::Milliseconds(200)));
  });
  scheduler->WaitUntilIdle();
  // The stopwatch should have measured ~200ms of wall clock time, since the
  // two threads will have run concurrently but there were three tasks, so the
  // third task will have run sequentially.
  EXPECT_THAT(stopwatch->GetTotalDuration(),
              AllOf(Ge(absl::Milliseconds(200)), Lt(absl::Milliseconds(250))));
}

/** Tests that the stopwatch handles stop/starts across different threads
 * correctly, including partially overlapping measurements. */
TEST(WallClockStopwatchTest, TwoThreadsMultipleOverlappingStartStop) {
  auto stopwatch = WallClockStopwatch::Create();
  std::unique_ptr<Scheduler> scheduler =
      CreateThreadPoolScheduler(/*thread_count=*/2);

  scheduler->Schedule([&stopwatch]() {
    auto started_stopwatch = stopwatch->Start();
    absl::SleepFor(absl::Milliseconds(100));
    EXPECT_THAT(stopwatch->GetTotalDuration(), Ge(absl::Milliseconds(100)));
  });
  scheduler->Schedule([&stopwatch]() {
    auto started_stopwatch = stopwatch->Start();
    absl::SleepFor(absl::Milliseconds(50));
    EXPECT_THAT(stopwatch->GetTotalDuration(), Ge(absl::Milliseconds(50)));
  });
  scheduler->Schedule([&stopwatch]() {
    auto started_stopwatch = stopwatch->Start();
    absl::SleepFor(absl::Milliseconds(50));
    EXPECT_THAT(stopwatch->GetTotalDuration(), Ge(absl::Milliseconds(100)));
  });
  scheduler->WaitUntilIdle();

  // The stopwatch should have measured ~100ms of wall clock time until now,
  // since the two threads will have run concurrently and there were three
  // tasks, which should all have been able to run concurrently within that
  // time.
  EXPECT_THAT(stopwatch->GetTotalDuration(),
              AllOf(Ge(absl::Milliseconds(100)), Lt(absl::Milliseconds(150))));

  absl::SleepFor(absl::Milliseconds(100));
  // The SleepFor should not be reflected in the measurement since all
  // stopwatches were stopped.
  EXPECT_THAT(stopwatch->GetTotalDuration(),
              AllOf(Ge(absl::Milliseconds(100)), Lt(absl::Milliseconds(150))));

  {
    auto outer_started_stopwatch = stopwatch->Start();
    absl::SleepFor(absl::Milliseconds(50));
    EXPECT_THAT(stopwatch->GetTotalDuration(), Ge(absl::Milliseconds(150)));
    scheduler->Schedule([&stopwatch]() {
      auto started_stopwatch = stopwatch->Start();
      absl::SleepFor(absl::Milliseconds(200));
      EXPECT_THAT(stopwatch->GetTotalDuration(), Ge(absl::Milliseconds(350)));
    });
    scheduler->Schedule([&stopwatch]() {
      auto started_stopwatch = stopwatch->Start();
      absl::SleepFor(absl::Milliseconds(50));
      EXPECT_THAT(stopwatch->GetTotalDuration(), Ge(absl::Milliseconds(200)));
    });
    scheduler->Schedule([&stopwatch]() {
      auto started_stopwatch = stopwatch->Start();
      absl::SleepFor(absl::Milliseconds(350));
      EXPECT_THAT(stopwatch->GetTotalDuration(), Ge(absl::Milliseconds(500)));
    });
    scheduler->WaitUntilIdle();

    // The stopwatch should have measured ~550ms of wall clock time until now:
    // the previous ~100ms measurement + 50ms + 50ms + 350ms (the shortest
    // critical path for the above three tasks).
    //
    // Note that the outer stopwatch is still active so the measurement is still
    // ongoing.
    EXPECT_THAT(
        stopwatch->GetTotalDuration(),
        AllOf(Ge(absl::Milliseconds(550)), Lt(absl::Milliseconds(600))));
    absl::SleepFor(absl::Milliseconds(100));
  }

  // The final SleepFor should now also be reflected.
  EXPECT_THAT(stopwatch->GetTotalDuration(),
              AllOf(Ge(absl::Milliseconds(650)), Lt(absl::Milliseconds(700))));
}

}  // namespace fcp
