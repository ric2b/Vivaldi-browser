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

#ifndef FCP_SECAGG_SERVER_SECAGG_SCHEDULER_H_
#define FCP_SECAGG_SERVER_SECAGG_SCHEDULER_H_

#include <atomic>
#include <functional>
#include <memory>
#include <utility>

#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "fcp/base/clock.h"
#include "fcp/base/monitoring.h"
#include "fcp/base/reentrancy_guard.h"
#include "fcp/base/scheduler.h"

namespace fcp {
namespace secagg {

// Simple callback waiter that runs the function on Wakeup.
class CallbackWaiter : public Clock::Waiter {
 public:
  explicit CallbackWaiter(std::function<void()> callback)
      : callback_(std::move(callback)) {}

  void WakeUp() override { callback_(); }

 private:
  std::function<void()> callback_;
};

// Provides async worker abstraction for SecAggScheduler.
class AsyncWorker {
 public:
  virtual ~AsyncWorker() = default;

  // Returns true if there is no any pending async work, even if the results
  // have not been observed by a callback.
  virtual bool IsIdle() = 0;

  // Returns false if no async work has happened since last time this function
  // was called, or the first time it is called. Otherwise it returns true and
  // schedules a callback to be called once the scheduler is idle.
  virtual bool SetAsyncObserver(std::function<void()> async_callback) = 0;

  // Calling Cancel results in skipping the remaining, still pending, async
  // work. The call blocks waiting for any currently active ongoing tasks to
  // complete. Calling Cancel for the second time has no additional effect.
  virtual void Cancel() = 0;
};

using AsyncToken = std::shared_ptr<AsyncWorker>;

template <typename T>
class Accumulator : public AsyncWorker,
                    public std::enable_shared_from_this<Accumulator<T>> {
 public:
  Accumulator(
      std::unique_ptr<T> initial_value,
      std::function<std::unique_ptr<T>(const T&, const T&)> accumulator_func,
      Scheduler* parallel_scheduler, Scheduler* sequential_scheduler,
      Clock* clock)
      : parallel_scheduler_(parallel_scheduler),
        sequential_scheduler_(sequential_scheduler),
        accumulated_value_(std::move(initial_value)),
        accumulator_func_(accumulator_func),
        clock_(clock) {}

  inline static std::function<void()> GetParallelScheduleFunc(
      std::shared_ptr<Accumulator<T>> accumulator,
      std::function<std::unique_ptr<T>()> generator) {
    return [accumulator, generator] {
      // Increment active count if the accumulator is not canceled, otherwise
      // return without scheduling the task. By active count we mean the total
      // number of scheduled tasks, both parallel and sequential. To cancel an
      // accumulator, we wait until that this count is 0.
      if (!accumulator->MaybeIncrementActiveCount()) {
        return;
      }
      auto partial = generator();
      FCP_CHECK(partial);
      // Decrement the count for the parallel task that was just run as
      // generator().
      accumulator->DecrementActiveCount();
      // Schedule sequential part of the generator, only if accumulator is not
      // cancelled, otherwise return without scheduling it.
      if (accumulator->IsCancelled()) {
        return;
      }
      accumulator->RunSequential(
          [=, partial = std::shared_ptr<T>(partial.release())] {
            ReentrancyGuard guard;
            FCP_CHECK_STATUS(guard.Check(accumulator->in_sequential_call()));
            // mark that a task will be
            // scheduled, if the accumulator is
            // not canceled.
            if (!accumulator->MaybeIncrementActiveCount()) {
              return;
            }
            auto new_value = accumulator->accumulator_func_(
                *accumulator->accumulated_value_, *partial);
            FCP_CHECK(new_value);
            accumulator->accumulated_value_ = std::move(new_value);
            // At this point the sequential task has been run, and we (i)
            // decrement both active and remaining counts and possibly reset the
            // unobserved work flag, (ii) get the callback, which might be
            // empty, and (iii) call it if that is not the case.
            auto callback = accumulator->UpdateCountsAndGetCallback();
            if (callback) {
              callback();
            }
          });
    };
  }

  // Schedule a parallel generator that includes a delay. The result of the
  // generator is fed to the accumulator_func
  void Schedule(std::function<std::unique_ptr<T>()> generator,
                absl::Duration delay) {
    // IncrementRemainingCount() keeps track of the number of async tasks
    // scheduled, and sets a flag when the count goes from 0 to 1, corresponding
    // to a starting batch of unobserved work.
    auto shared_this = this->shared_from_this();
    shared_this->IncrementRemainingCount();
    clock_->WakeupWithDeadline(
        clock_->Now() + delay,
        std::make_shared<CallbackWaiter>([shared_this, generator] {
          shared_this->RunParallel(
              Accumulator<T>::GetParallelScheduleFunc(shared_this, generator));
        }));
  }

  // Schedule a parallel generator. The result of the generator is fed to the
  // accumulator_func
  void Schedule(std::function<std::unique_ptr<T>()> generator) {
    // IncrementRemainingCount() keeps track of the number of async tasks
    // scheduled, and sets a flag when the count goes from 0 to 1, corresponding
    // to a starting batch of unobserved work.
    auto shared_this = this->shared_from_this();
    shared_this->IncrementRemainingCount();
    RunParallel([shared_this, generator] {
      shared_this->GetParallelScheduleFunc(shared_this, generator)();
    });
  }

  // AsyncWorker implementation.
  // Returns true if the accumulator doesn't have any remaining tasks,
  // even if their results have not been observed by a callback.
  bool IsIdle() override {
    absl::MutexLock lock(&mutex_);
    return remaining_sequential_tasks_count_ == 0;
  }

  // AsyncWorker implementation.
  // Returns false if no async work has happened since last time this function
  // was called, or the first time it is called. Otherwise it returns true and
  // schedules a callback to be called once the scheduler is idle.
  bool SetAsyncObserver(std::function<void()> async_callback) override {
    bool idle;
    {
      absl::MutexLock lock(&mutex_);
      if (!has_unobserved_work_) {
        return false;
      }
      idle = (remaining_sequential_tasks_count_ == 0);
      if (idle) {
        // The flag is set to false, and the callback is run as soon as we leave
        // the mutex's scope.
        has_unobserved_work_ = false;
      } else {
        // The callbak is scheduled for later, as there is ongoing work.
        async_callback_ = async_callback;
      }
    }
    if (idle) {
      auto shared_this = this->shared_from_this();
      RunSequential([async_callback, shared_this] { async_callback(); });
    }
    return true;
  }

  // Updates the active and remaining task counts, and returns the callback to
  // be executed, or nullptr if there's pending async work.
  inline std::function<void()> UpdateCountsAndGetCallback() {
    absl::MutexLock lock(&mutex_);
    if (--active_count_ == 0 && is_cancelled_) {
      inactive_cv_.SignalAll();
    }
    --remaining_sequential_tasks_count_;
    if (remaining_sequential_tasks_count_ == 0 && async_callback_) {
      has_unobserved_work_ = false;
      auto callback = async_callback_;
      async_callback_ = nullptr;
      return callback;
    } else {
      return nullptr;
    }
  }

  // Take the accumulated result and abort any further work. This method can
  // only be called when the accumulator is idle
  std::unique_ptr<T> GetResultAndCancel() {
    absl::MutexLock lock(&mutex_);
    FCP_CHECK(active_count_ == 0);
    is_cancelled_ = true;
    return std::move(accumulated_value_);
  }

  // AsyncWorker implementation
  void Cancel() override {
    mutex_.Lock();
    is_cancelled_ = true;
    while (active_count_ > 0) {
      inactive_cv_.Wait(&mutex_);
    }
    mutex_.Unlock();
  }

  bool IsCancelled() {
    absl::MutexLock lock(&mutex_);
    return is_cancelled_;
  }

  bool MaybeIncrementActiveCount() {
    absl::MutexLock lock(&mutex_);
    if (is_cancelled_) {
      return false;
    }
    active_count_++;
    return true;
  }

  size_t DecrementActiveCount() {
    absl::MutexLock lock(&mutex_);
    FCP_CHECK(active_count_ > 0);
    if (--active_count_ == 0 && is_cancelled_) {
      inactive_cv_.SignalAll();
    }
    return active_count_;
  }

  void IncrementRemainingCount() {
    absl::MutexLock lock(&mutex_);
    has_unobserved_work_ |= (remaining_sequential_tasks_count_ == 0);
    remaining_sequential_tasks_count_++;
  }

  std::atomic<bool>* in_sequential_call() { return &in_sequential_call_; }

  void inline RunParallel(std::function<void()> function) {
    parallel_scheduler_->Schedule(function);
  }

  void inline RunSequential(std::function<void()> function) {
    sequential_scheduler_->Schedule(function);
  }

 private:
  // Scheduler for sequential and parallel tasks, received from the
  // SecAggScheduler instatiating this class
  Scheduler* parallel_scheduler_;
  Scheduler* sequential_scheduler_;

  // Callback to be executed the next time that the sequential scheduler
  // becomes idle.
  std::function<void()> async_callback_ ABSL_GUARDED_BY(mutex_) =
      std::function<void()>();
  // Accumulated value - accessed by sequential tasks only.
  std::unique_ptr<T> accumulated_value_;
  // Accumulation function - accessed by sequential tasks only.
  std::function<std::unique_ptr<T>(const T&, const T&)> accumulator_func_;
  // Clock used for scheduling delays in parallel tasks
  Clock* clock_;
  // Remaining number of sequential tasks to be executed - accessed by
  // sequential tasks only.
  size_t remaining_sequential_tasks_count_ ABSL_GUARDED_BY(mutex_) = 0;
  bool has_unobserved_work_ ABSL_GUARDED_BY(mutex_) = false;

  // Number of active calls to either callback function.
  size_t active_count_ ABSL_GUARDED_BY(mutex_) = 0;
  // This is set to true when the run is aborted.
  bool is_cancelled_ ABSL_GUARDED_BY(mutex_) = false;
  // Protects active_count_ and cancelled_.
  absl::Mutex mutex_;
  // Used to notify cancellation about reaching inactive state;
  absl::CondVar inactive_cv_;
  // This is used by ReentrancyGuard to ensure that Sequential tasks are
  // indeed sequential.
  std::atomic<bool> in_sequential_call_ = false;
};

// Implementation of ParallelGenerateSequentialReduce based on fcp::Scheduler.
// Takes two Schedulers, one which is responsible for parallel execution and
// another for serial execution. Additionally, takes a clock that can be used to
// induce delay in task executions.
class SecAggScheduler {
 public:
  SecAggScheduler(Scheduler* parallel_scheduler,
                  Scheduler* sequential_scheduler,
                  Clock* clock = Clock::RealClock())
      : parallel_scheduler_(parallel_scheduler),
        sequential_scheduler_(sequential_scheduler),
        clock_(clock) {}

  // SecAggScheduler is neither copyable nor movable.
  SecAggScheduler(const SecAggScheduler&) = delete;
  SecAggScheduler& operator=(const SecAggScheduler&) = delete;

  virtual ~SecAggScheduler() = default;

  // Schedule a callback to be invoked on the sequential scheduler.
  inline void ScheduleCallback(std::function<void()> callback) {
    RunSequential(callback);
  }

  template <typename T>
  std::shared_ptr<Accumulator<T>> CreateAccumulator(
      std::unique_ptr<T> initial_value,
      std::function<std::unique_ptr<T>(const T&, const T&)> accumulator_func) {
    return std::make_shared<Accumulator<T>>(
        std::move(initial_value), accumulator_func, parallel_scheduler_,
        sequential_scheduler_, clock_);
  }

  void WaitUntilIdle();

 protected:
  // Virtual for testing
  virtual void RunSequential(std::function<void()> function);

 private:
  Scheduler* parallel_scheduler_;
  Scheduler* sequential_scheduler_;
  Clock* clock_;
};

}  // namespace secagg
}  // namespace fcp

#endif  // FCP_SECAGG_SERVER_SECAGG_SCHEDULER_H_
