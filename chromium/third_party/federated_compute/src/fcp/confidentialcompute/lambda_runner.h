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

#ifndef FCP_CONFIDENTIALCOMPUTE_LAMBDA_RUNNER_H_
#define FCP_CONFIDENTIALCOMPUTE_LAMBDA_RUNNER_H_

#include <cstdint>
#include <optional>

#include "absl/status/statusor.h"
#include "tensorflow_federated/proto/v0/executor.pb.h"

namespace fcp::confidential_compute {

class LambdaRunner {
 public:
  virtual ~LambdaRunner() = default;

  // Synchronously runs a function over an input arg and returns the result.
  //
  // A LambdaRunner is not itself a TFF Executor; instead it abstracts away
  // functionality that is required by the TEE executor operating at a leaf
  // node within a hierarchy of TFF Executors.
  //
  // The first parameter is guaranteed to be a TFF value of type Computation of
  // type Lambda. The num_clients parameter indicates how many clients are
  // expected to be provided via the arg parameter and can be used by configure
  // any helper executors that require this field (e.g. FederatingExecutor).
  virtual absl::StatusOr<tensorflow_federated::v0::Value> ExecuteComp(
      tensorflow_federated::v0::Value function,
      std::optional<tensorflow_federated::v0::Value> arg,
      int32_t num_clients) = 0;
};

}  // namespace fcp::confidential_compute

#endif  // FCP_CONFIDENTIALCOMPUTE_LAMBDA_RUNNER_H_
