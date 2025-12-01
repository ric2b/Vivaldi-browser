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

#include "fcp/base/future.h"

#include <functional>
#include <memory>
#include <thread>  // NOLINT(build/c++11)
#include <type_traits>
#include <utility>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/base/thread_annotations.h"
#include "absl/synchronization/barrier.h"
#include "absl/time/time.h"
#include "fcp/base/meta.h"
#include "fcp/base/move_to_lambda.h"

namespace fcp {
namespace thread {

using ::testing::Eq;

// Future::Wait and Future::Take sometimes block. We'd like to test thread
// interleavings where these calls block before being woken up. In the absence
// of instrumentation of the underlying synchronization primitives, we just use
// this arbitrary delay before unblocking operations (the other thread
// "probably" has time to block). Note that the other ordering (Promise::Set
// before Future::Take etc.) is easy to guarantee.
constexpr absl::Duration kArbitraryDelay = absl::Milliseconds(50);

void Delay() { absl::SleepFor(kArbitraryDelay); }

// Freely copyable test value. We put in a 'valid' value and hope to find it
// again.
enum class V { kInvalid, kValid };

// Move-only test value (we use this for Promise and Future, to make sure they
// are compatible with move-only types - typically the harder case). This
// corresponds to V:
//   Value present (not moved) <=> kValid
//   Value moved <=> kInvalid
// The TakeV / SetV wrappers below actually do those conversions, since the test
// assertions (e.g. Eq matcher) are difficult to use with move-only types.
using UV = UniqueValue<Unit>;

static_assert(!std::is_copy_constructible<UV>::value,
              "Expected to be move-only");

std::optional<V> TakeV(Future<UV> future) {
  std::optional<UV> maybe_uv = std::move(future).Take();
  if (maybe_uv.has_value()) {
    UniqueValue<Unit> uv = *std::move(maybe_uv);
    return uv.has_value() ? V::kValid : V::kInvalid;
  } else {
    return std::nullopt;
  }
}

void SetV(Promise<UV> promise) { std::move(promise).Set(UV(Unit{})); }

absl::Barrier MakeBarrier() { return absl::Barrier(2); }

void RunThreads(std::vector<std::function<void()>> fns) {
  std::vector<std::thread> threads;
  for (auto& fn : fns) {
    threads.push_back(std::thread(std::move(fn)));
  }

  for (auto& thread : threads) {
    thread.join();
  }
}

void RunThreadsWithFuture(std::function<void(Promise<UV>)> promise_fn,
                          std::function<void(Future<UV>)> future_fn) {
  FuturePair<UV> pair = MakeFuture<UV>();

  MoveToLambdaWrapper<Promise<UV>> promise_capture =
      MoveToLambda(std::move(pair.promise));
  auto promise_fn_wrapped = [promise_capture, promise_fn]() mutable {
    promise_fn(std::move(*promise_capture));
  };

  MoveToLambdaWrapper<Future<UV>> future_capture =
      MoveToLambda(std::move(pair.future));
  auto future_fn_wrapped = [future_capture, future_fn]() mutable {
    future_fn(std::move(*future_capture));
  };

  RunThreads({std::move(promise_fn_wrapped), std::move(future_fn_wrapped)});
}

TEST(FutureTest, WaitTimeouts) {
  absl::Barrier waited = MakeBarrier();
  absl::Barrier set = MakeBarrier();

  auto promise_fn = [&](Promise<UV> promise) {
    waited.Block();
    SetV(std::move(promise));
    set.Block();
  };

  auto future_fn = [&](Future<UV> future) {
    // Before set: Timeout should elapse
    EXPECT_FALSE(future.Wait(absl::Milliseconds(1)))
        << "Future shouldn't be ready yet";
    waited.Block();
    set.Block();
    // After set: Zero timeout should be sufficient
    EXPECT_TRUE(future.Wait(absl::ZeroDuration()))
        << "Future should be ready without waiting";
  };

  RunThreadsWithFuture(std::move(promise_fn), std::move(future_fn));
}

TEST(FutureTest, TakeAfterSet) {
  absl::Barrier set = MakeBarrier();

  auto promise_fn = [&](Promise<UV> promise) {
    SetV(std::move(promise));
    set.Block();
  };

  auto future_fn = [&](Future<UV> future) {
    set.Block();
    EXPECT_THAT(TakeV(std::move(future)), Eq(V::kValid));
  };

  RunThreadsWithFuture(std::move(promise_fn), std::move(future_fn));
}

TEST(FutureTest, TakeProbablyBeforeSet) {
  auto promise_fn = [](Promise<UV> promise) {
    Delay();
    SetV(std::move(promise));
  };

  auto future_fn = [](Future<UV> future) {
    EXPECT_THAT(TakeV(std::move(future)), Eq(V::kValid));
  };

  RunThreadsWithFuture(std::move(promise_fn), std::move(future_fn));
}

TEST(FutureTest, AbandonWhileProbablyTaking) {
  auto promise_fn = [](Promise<UV> promise) {
    Delay();
    { Promise<UV> dies = std::move(promise); }
  };

  auto future_fn = [](Future<UV> future) {
    EXPECT_THAT(std::move(future).Take(), Eq(std::nullopt));
  };

  RunThreadsWithFuture(std::move(promise_fn), std::move(future_fn));
}

TEST(FutureTest, SetWhileProbablyWaiting) {
  auto promise_fn = [](Promise<UV> promise) {
    Delay();
    SetV(std::move(promise));
  };

  auto future_fn = [](Future<UV> future) {
    EXPECT_TRUE(future.Wait(absl::InfiniteDuration()));
  };

  RunThreadsWithFuture(std::move(promise_fn), std::move(future_fn));
}

TEST(FutureTest, AbandonWhileProbablyWaiting) {
  auto promise_fn = [](Promise<UV> promise) {
    Delay();
    { Promise<UV> dies = std::move(promise); }
  };

  auto future_fn = [](Future<UV> future) {
    EXPECT_TRUE(future.Wait(absl::InfiniteDuration()));
  };

  RunThreadsWithFuture(std::move(promise_fn), std::move(future_fn));
}

}  // namespace thread
}  // namespace fcp
