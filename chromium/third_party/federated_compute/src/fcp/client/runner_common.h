/*
 * Copyright 2024 Google LLC
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
#ifndef FCP_CLIENT_RUNNER_COMMON_H_
#define FCP_CLIENT_RUNNER_COMMON_H_

#include <string>
#include <utility>

#include "fcp/client/engine/common.h"
namespace fcp::client {

struct PlanResultAndCheckpointFile {
  explicit PlanResultAndCheckpointFile(engine::PlanResult plan_result)
      : plan_result(std::move(plan_result)) {}
  engine::PlanResult plan_result;
  // The name of the output checkpoint file. Empty if the plan did not produce
  // an output checkpoint.
  std::string checkpoint_filename;

  PlanResultAndCheckpointFile(PlanResultAndCheckpointFile&&) = default;
  PlanResultAndCheckpointFile& operator=(PlanResultAndCheckpointFile&&) =
      default;

  // Disallow copy and assign.
  PlanResultAndCheckpointFile(const PlanResultAndCheckpointFile&) = delete;
  PlanResultAndCheckpointFile& operator=(const PlanResultAndCheckpointFile&) =
      delete;
};

}  // namespace fcp::client

#endif  // FCP_CLIENT_RUNNER_COMMON_H_
