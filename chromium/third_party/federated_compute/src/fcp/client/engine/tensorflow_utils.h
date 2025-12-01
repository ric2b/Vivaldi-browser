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
#ifndef FCP_CLIENT_ENGINE_TENSORFLOW_UTILS_H_
#define FCP_CLIENT_ENGINE_TENSORFLOW_UTILS_H_

#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/statusor.h"
#include "fcp/client/federated_protocol.h"
#include "fcp/protos/federated_api.pb.h"
#include "fcp/protos/plan.pb.h"
#include "tensorflow/core/framework/tensor.h"

namespace fcp::client::engine {

// Parses the output of an eligibility eval plan.
//
// The output of an eligibility eval plan is a single scalar tensor of dtype
// string. The value of the tensor is a serialized
// google.internal.federatedml.v2.TaskEligibilityInfo proto.
absl::StatusOr<google::internal::federatedml::v2::TaskEligibilityInfo>
ParseEligibilityEvalPlanOutput(
    const std::vector<tensorflow::Tensor>& output_tensors);

// Converts a vector of tensor name and a vector of tensorflow::Tensor to a
// map of tensor name to QuantizedTensor.  The shape of the tensor is taken from
// the TensorflowSpec.
absl::StatusOr<absl::flat_hash_map<std::string, QuantizedTensor>>
CreateQuantizedTensorMap(
    const std::vector<std::string>& tensor_names,
    const std::vector<tensorflow::Tensor>& tensors,
    const google::internal::federated::plan::TensorflowSpec& tensorflow_spec);

}  // namespace fcp::client::engine

#endif  // FCP_CLIENT_ENGINE_TENSORFLOW_UTILS_H_
