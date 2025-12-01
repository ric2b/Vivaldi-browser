// This file is part of Eigen, a lightweight C++ template library
// for linear algebra.
//
// Copyright (C) 2025 Weiwei Kong <weiweikong@google.com>
//
// This Source Code Form is subject to the terms of the Mozilla
// Public License v. 2.0. If a copy of the MPL was not distributed
// with this file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef EIGEN_THREADPOOL_FORKJOIN_H
#define EIGEN_THREADPOOL_FORKJOIN_H

// IWYU pragma: private
#include "./InternalHeaderCheck.h"

namespace Eigen {

// ForkJoinScheduler provides implementations of various non-blocking ParallelFor algorithms for unary
// and binary parallel tasks. More specfically, the implementations follow the binary tree-based
// algorithm from the following paper:
//
//   Lea, D. (2000, June). A java fork/join framework. *In Proceedings of the
//   ACM 2000 conference on Java Grande* (pp. 36-43).
//
// For a given binary task function `f(i,j)` and integers `num_threads`, `granularity`, `start`, and `end`,
// the implemented parallel for algorithm schedules and excutes at most `num_threads` of the functions
// from the following set in parallel (either synchronously or asynchronously):
//
//   f(start,start+s_1), f(start+s_1,start+s_2), ..., f(start+s_n,end)
//
// where `s_{j+1} - s_{j}` and `end - s_n` are roughly within a factor of two of `granularity`. For a unary
// task function `g(k)`, the same operation is applied with
//
//   f(i,j) = [&](){ for(int k = i; k < j; ++k) g(k); };
//
// Note that the parameter `granularity` should be tuned by the user based on the trade-off of running the
// given task function sequentially vs. scheduling individual tasks in parallel. An example of a partially
// tuned `granularity` is in `Eigen::CoreThreadPoolDevice::parallelFor(...)` where the template
// parameter `PacketSize` and float input `cost` are used to indirectly compute a granularity level for a
// given task function.
//
// Example usage #1 (synchronous):
// ```
// ThreadPool thread_pool(num_threads);
// ForkJoinScheduler::ParallelFor(0, num_tasks, granularity, std::move(parallel_task), &thread_pool);
// ```
//
// Example usage #2 (asynchronous):
// ```
// ThreadPool thread_pool(num_threads);
// Barrier barrier(num_tasks * num_async_calls);
// auto done = [&](){barrier.Notify();};
// for (int k=0; k<num_async_calls; ++k) {
//   thread_pool.Schedule([&](){
//     ForkJoinScheduler::ParallelForAsync(0, num_tasks, granularity, parallel_task, done, &thread_pool);
//   });
// }
// barrier.Wait();
// ```
class ForkJoinScheduler {
 public:
  // Runs `do_func` asynchronously for the range [start, end) with a specified granularity. `do_func` should
  // either be of type `std::function<void(int)>` or `std::function<void(int, int)`.
  // If `end > start`, the `done` callback will be called `end - start` times when all tasks have been
  // executed. Otherwise, `done` is called only once.
  template <typename DoFnType>
  static void ParallelForAsync(int start, int end, int granularity, DoFnType do_func, std::function<void()> done,
                               Eigen::ThreadPool* thread_pool) {
    if (start >= end) {
      done();
      return;
    }
    ForkJoinScheduler::RunParallelForAsync(start, end, granularity, do_func, done, thread_pool);
  }

  // Synchronous variant of ParallelForAsync.
  template <typename DoFnType>
  static void ParallelFor(int start, int end, int granularity, DoFnType do_func, Eigen::ThreadPool* thread_pool) {
    if (start >= end) return;
    auto dummy_done = []() {};
    Barrier barrier(1);
    thread_pool->Schedule([start, end, granularity, thread_pool, &do_func, &dummy_done, &barrier]() {
      ForkJoinScheduler::ParallelForAsync(start, end, granularity, do_func, dummy_done, thread_pool);
      barrier.Notify();
    });
    barrier.Wait();
  }

 private:
  // Schedules `right_thunk`, runs `left_thunk`, and runs other tasks until `right_thunk` has finished.
  template <typename LeftType, typename RightType>
  static void ForkJoin(LeftType&& left_thunk, RightType&& right_thunk, Eigen::ThreadPool* thread_pool) {
    std::atomic<bool> right_done(false);
    auto execute_right = [&right_thunk, &right_done]() {
      std::forward<RightType>(right_thunk)();
      right_done.store(true, std::memory_order_release);
    };
    thread_pool->Schedule(execute_right);
    std::forward<LeftType>(left_thunk)();
    Eigen::ThreadPool::Task task;
    while (!right_done.load(std::memory_order_acquire)) {
      thread_pool->MaybeGetTask(&task);
      if (task.f) task.f();
    }
  }

  // Runs `do_func` in parallel for the range [start, end). The main recursive asynchronous runner that
  // calls `ForkJoin`.
  static void RunParallelForAsync(int start, int end, int granularity, std::function<void(int)>& do_func,
                                  std::function<void()>& done, Eigen::ThreadPool* thread_pool) {
    std::function<void(int, int)> wrapped_do_func = [&do_func](int start, int end) {
      for (int i = start; i < end; ++i) do_func(i);
    };
    ForkJoinScheduler::RunParallelForAsync(start, end, granularity, wrapped_do_func, done, thread_pool);
  }

  // Variant of `RunAsyncParallelFor` that uses a do function that operates on an index range.
  // Specifically, `do_func` takes two arguments: the start and end of the range.
  static void RunParallelForAsync(int start, int end, int granularity, std::function<void(int, int)>& do_func,
                                  std::function<void()>& done, Eigen::ThreadPool* thread_pool) {
    if ((end - start) <= granularity) {
      do_func(start, end);
      for (int j = 0; j < end - start; ++j) done();
    } else {
      // Typical workloads choose initial values of `{start, end, granularity}` such that `start - end` and
      // `granularity` are powers of two. Since modern processors usually implement (2^x)-way
      // set-associative caches, we minimize the number of cache misses by choosing midpoints that are not
      // powers of two (to avoid having two addresses in the main memory pointing to the same point in the
      // cache). More specifically, we restrict the set of candidate midpoints to:
      //
      //   P := {start, start + granularity, start + 2*granularity, ..., end},
      //
      // and choose the entry in `P` at (roughly) the 9/16 mark.
      const int size = end - start;
      const int mid = start + Eigen::numext::div_ceil(9 * (size + 1) / 16, granularity) * granularity;
      ForkJoinScheduler::ForkJoin(
          [start, mid, granularity, &do_func, &done, thread_pool]() {
            RunParallelForAsync(start, mid, granularity, do_func, done, thread_pool);
          },
          [mid, end, granularity, &do_func, &done, thread_pool]() {
            RunParallelForAsync(mid, end, granularity, do_func, done, thread_pool);
          },
          thread_pool);
    }
  }
};

}  // namespace Eigen

#endif  // EIGEN_THREADPOOL_FORKJOIN_H
