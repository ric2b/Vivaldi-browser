/*
 * Copyright 2025 Google LLC
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

#ifndef FCP_CONFIDENTIALCOMPUTE_COMPOSING_TEE_EXECUTOR_H_
#define FCP_CONFIDENTIALCOMPUTE_COMPOSING_TEE_EXECUTOR_H_

#include <memory>
#include <vector>

#include "tensorflow_federated/cc/core/impl/executors/composing_executor.h"
#include "tensorflow_federated/cc/core/impl/executors/executor.h"

namespace fcp {
namespace confidential_compute {

// The ComposingTeeExecutor is a TFF executor that implements the "composed_tee"
// intrinsic by delegating logic to one server child executor and many client
// child executors.

// The "composed_tee" intrinsic is a function that takes as input a struct with
// four args: [accumulate_arg, partial_report_arg, accumulate_fn, report_fn].
//
// The ComposingTeeExecutor implements a "composed_tee" intrinsic by
// distributing the logic across the child executors as follows:
//
//  1. For each client child executor:
//    a. Run accumulate_fn over the parts of accumulate_arg assigned to that
//       client child executor.
//    b. The client child executor will return a value (potentially a Data
//       computation) representing a struct with two values: pre-aggregate
//       values and partial aggregate values. Pre-aggregate values are values
//       that may be consumed by a future accumulate function in a separate
//       "composed_tee" call, whereas partial aggregate values are values that
//       have already been partially aggregated within the accumulate function
//       and should be consumed by the report function in the same
//       "composed_tee" call.
//  2. For the server child executor:
//    a. Run report_fn over a struct with two elements: a struct containing the
//       partial aggregate values produced by each client child executor in
//       step 1b, and the partial_report_arg provided in the original
//       "composed_tee" arg list.
//    b. The server child executor will return a single value (potentially a
//       Data computation).
//  3. Return a struct with two elements: a CLIENTS-placed federated value
//     containing the pre-aggregate values produced by each client child
//     executor in step 1b, and the value returned by the server child executor
//     in step 2b.

inline constexpr int kPreAggregateOutputIndex = 0;
inline constexpr int kPartialAggregateOutputIndex = 1;

// Creates a ComposingTeeExecutor using the provided server and client child
// executors.
std::shared_ptr<tensorflow_federated::Executor> CreateComposingTeeExecutor(
    std::shared_ptr<tensorflow_federated::Executor> server,
    std::vector<tensorflow_federated::ComposingChild> clients);

}  // namespace confidential_compute
}  // namespace fcp

#endif  // FCP_CONFIDENTIALCOMPUTE_COMPOSING_TEE_EXECUTOR_H_
