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

#ifndef FCP_BASE_WALL_CLOCK_STOPWATCH_H_
#define FCP_BASE_WALL_CLOCK_STOPWATCH_H_

#include <memory>

#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"

namespace fcp {

namespace internal {
class RealWallClockStopwatch;
class NoopWallClockStopwatch;
}  // namespace internal

// A utility for measuring wall clock time across multiple threads.
//
// This class is non-reentrant: `Start()` should only be called once per thread
// (but once `Stop()` has been called, `Start()` may be called again).
class WallClockStopwatch {
 public:
  static std::unique_ptr<WallClockStopwatch> Create();
  static std::unique_ptr<WallClockStopwatch> CreateNoop();
  // Disable copy and move semantics.
  WallClockStopwatch(const WallClockStopwatch&) = delete;
  WallClockStopwatch& operator=(const WallClockStopwatch&) = delete;

  // A handle that stops the stopwatch once destroyed.
  class Handle {
   public:
    // Disable copy and move semantics.
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    ~Handle();

   private:
    // If `stopwatch` is a nullptr then the Handle that does nothing (for use in
    // testing or flagging-off the measurement with a real stopwatch).
    explicit Handle(WallClockStopwatch* stopwatch);

    WallClockStopwatch* const stopwatch_;
    friend internal::RealWallClockStopwatch;
    friend internal::NoopWallClockStopwatch;
  };

  // Start the stopwatch from this thread. If it wasn't running yet from any
  // other thread, then time will start being accumulated from this point on.
  // If it was already running from another thread then this call will have no
  // immediate effect.
  //
  // Once the returned Handle is destroyed, the stopwatch is stopped from this
  // thread. If it isn't running from any other thread, then time will stop
  // being accumulated from that point on. If it still running from another
  // thread then Handle destruction will have no immediate effect.
  virtual Handle Start() = 0;

  // Get the total duration of wall clock time that the stopwatch has run for,
  // up until this moment (i.e. including any still-ongoing measurement).
  virtual absl::Duration GetTotalDuration() const = 0;

  virtual ~WallClockStopwatch() = default;

 private:
  WallClockStopwatch() = default;
  virtual void StartInternal() {}
  virtual void StopInternal() {}
  friend internal::RealWallClockStopwatch;
  friend internal::NoopWallClockStopwatch;
};

}  // namespace fcp

#endif  // FCP_BASE_WALL_CLOCK_STOPWATCH_H_
