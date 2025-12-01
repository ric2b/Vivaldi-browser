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

#ifndef FCP_SECAGG_TESTING_SERVER_TEST_ASYNC_RUNNER_H_
#define FCP_SECAGG_TESTING_SERVER_TEST_ASYNC_RUNNER_H_

#include <memory>
#include <utility>

#include "fcp/base/scheduler.h"
#include "fcp/secagg/server/secagg_scheduler.h"

namespace fcp {
namespace secagg {

// Defines a scheduler used for testing, owning pointers to underlying
// schedulers in SecAggScheduler
class TestAsyncRunner : public SecAggScheduler {
 public:
  TestAsyncRunner(std::unique_ptr<Scheduler> worker_scheduler,
                  std::unique_ptr<Scheduler> callback_scheduler)
      : SecAggScheduler(worker_scheduler.get(), callback_scheduler.get()),
        worker_scheduler_(std::move(worker_scheduler)),
        callback_scheduler_(std::move(callback_scheduler)) {}

  ~TestAsyncRunner() override { WaitUntilIdle(); }

 private:
  std::unique_ptr<Scheduler> worker_scheduler_;
  std::unique_ptr<Scheduler> callback_scheduler_;
};

}  // namespace secagg
}  // namespace fcp

#endif  // FCP_SECAGG_TESTING_SERVER_TEST_ASYNC_RUNNER_H_
