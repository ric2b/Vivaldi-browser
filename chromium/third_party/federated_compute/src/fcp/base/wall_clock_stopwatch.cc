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

#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "fcp/base/monitoring.h"

namespace fcp {

namespace internal {
class RealWallClockStopwatch : public WallClockStopwatch {
 public:
  RealWallClockStopwatch() = default;

  Handle Start() override ABSL_LOCKS_EXCLUDED(mutex_) {
    return WallClockStopwatch::Handle(this);
  }
  absl::Duration GetTotalDuration() const override ABSL_LOCKS_EXCLUDED(mutex_) {
    absl::MutexLock lock(&mutex_);
    FCP_CHECK(started_count_ >= 0);
    if (latest_start_time_ == absl::InfiniteFuture()) {
      return previous_durations_;
    }
    return previous_durations_ + (absl::Now() - latest_start_time_);
  }

 private:
  void StartInternal() override ABSL_LOCKS_EXCLUDED(mutex_) {
    absl::MutexLock lock(&mutex_);
    FCP_CHECK(started_count_ >= 0);
    started_count_++;
    if (started_count_ == 1) {
      latest_start_time_ = absl::Now();
    }
  }
  void StopInternal() override ABSL_LOCKS_EXCLUDED(mutex_) {
    absl::MutexLock lock(&mutex_);
    FCP_CHECK(started_count_ >= 1);
    started_count_--;
    if (started_count_ == 0) {
      previous_durations_ += absl::Now() - latest_start_time_;
      latest_start_time_ = absl::InfiniteFuture();
    }
  }

  mutable absl::Mutex mutex_;
  int started_count_ ABSL_GUARDED_BY(mutex_) = 0;
  absl::Time latest_start_time_ ABSL_GUARDED_BY(mutex_) =
      absl::InfiniteFuture();
  absl::Duration previous_durations_ ABSL_GUARDED_BY(mutex_) =
      absl::ZeroDuration();
};

// A noop stopwatch that does nothing (e.g. for use in tests or to
// flag-off the measurement of something).
class NoopWallClockStopwatch : public WallClockStopwatch {
 public:
  NoopWallClockStopwatch() = default;

  Handle Start() override { return Handle(nullptr); }
  absl::Duration GetTotalDuration() const override {
    return absl::ZeroDuration();
  }
};
}  // namespace internal

WallClockStopwatch::Handle::Handle(WallClockStopwatch* stopwatch)
    : stopwatch_(stopwatch) {
  if (stopwatch_ != nullptr) {
    stopwatch_->StartInternal();
  }
}

WallClockStopwatch::Handle::~Handle() {
  if (stopwatch_ != nullptr) {
    stopwatch_->StopInternal();
  }
}

std::unique_ptr<WallClockStopwatch> WallClockStopwatch::Create() {
  return std::make_unique<internal::RealWallClockStopwatch>();
}

std::unique_ptr<WallClockStopwatch> WallClockStopwatch::CreateNoop() {
  return std::make_unique<internal::NoopWallClockStopwatch>();
}

}  // namespace fcp
