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

#ifndef FCP_CONFIDENTIALCOMPUTE_TEE_EXECUTOR_H_
#define FCP_CONFIDENTIALCOMPUTE_TEE_EXECUTOR_H_

#include <cstdint>
#include <memory>

#include "fcp/confidentialcompute/lambda_runner.h"
#include "tensorflow_federated/cc/core/impl/executors/executor.h"

namespace fcp::confidential_compute {

// The TeeExecutor is a TFF executor that implements the "composed_tee/leaf"
// and "composed_tee/root" intrinsics by delegating logic to a LambdaRunner.
//
// The "composed_tee/leaf" amd "composed_tee/root" intrinsics both take as input
// a struct with two args: [arg, fn]. The TeeExecutor implements calls involving
// these intrinsics by asking the LambdaRunner to run `fn(arg)`.
//
// Even though both intrinsics currently supported by the TeeExecutor have the
// same signature, we use intrinsic uris within the TeeExecutor to allow for
// future divergence in the implementation of these intrinsics or introduction
// of others that may have different signatures.

// Creates a TeeExecutor using the provided lambda runner.
std::shared_ptr<tensorflow_federated::Executor> CreateTeeExecutor(
    std::shared_ptr<LambdaRunner> lambda_runner, int32_t num_clients);

}  // namespace fcp::confidential_compute

#endif  // FCP_CONFIDENTIALCOMPUTE_TEE_EXECUTOR_H_
