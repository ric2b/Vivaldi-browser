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

#ifndef FCP_CONFIDENTIALCOMPUTE_MOCK_LAMBDA_RUNNER_H_
#define FCP_CONFIDENTIALCOMPUTE_MOCK_LAMBDA_RUNNER_H_

#include <cstdint>
#include <optional>

#include "gmock/gmock.h"
#include "absl/status/statusor.h"
#include "fcp/confidentialcompute/lambda_runner.h"

namespace fcp::confidential_compute {

class MockLambdaRunner : public LambdaRunner {
 public:
  MOCK_METHOD(absl::StatusOr<tensorflow_federated::v0::Value>, ExecuteComp,
              (tensorflow_federated::v0::Value function,
               std::optional<tensorflow_federated::v0::Value> arg,
               int32_t num_clients),
              (override));
};

}  // namespace fcp::confidential_compute

#endif  // FCP_CONFIDENTIALCOMPUTE_MOCK_LAMBDA_RUNNER_H_
