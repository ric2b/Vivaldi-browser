/*
 * Copyright 2021 Google LLC
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

#include <functional>

namespace fcp {
namespace secagg {

void SecAggScheduler::WaitUntilIdle() {
  parallel_scheduler_->WaitUntilIdle();
  sequential_scheduler_->WaitUntilIdle();
}

void SecAggScheduler::RunSequential(std::function<void()> function) {
  sequential_scheduler_->Schedule(function);
}

}  // namespace secagg
}  // namespace fcp
