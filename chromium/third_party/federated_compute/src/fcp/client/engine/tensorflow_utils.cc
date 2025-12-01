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
#include "fcp/client/engine/tensorflow_utils.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "fcp/client/federated_protocol.h"
#include "fcp/protos/federated_api.pb.h"
#include "fcp/protos/plan.pb.h"
#include "tensorflow/core/framework/tensor.h"
#include "tensorflow/core/framework/tensor_shape.pb.h"
#include "tensorflow/core/platform/tstring.h"
#include "tensorflow/core/protobuf/struct.pb.h"

namespace fcp::client::engine {

using ::google::internal::federated::plan::TensorflowSpec;
using ::google::internal::federatedml::v2::TaskEligibilityInfo;

template <typename T>
void AddValuesToQuantized(QuantizedTensor* quantized,
                          const tensorflow::Tensor& tensor) {
  auto flat_tensor = tensor.flat<T>();
  quantized->values.reserve(quantized->values.size() + flat_tensor.size());
  for (int i = 0; i < flat_tensor.size(); i++) {
    quantized->values.push_back(flat_tensor(i));
  }
}

// Converts a tensorflow::Tensor to a QuantizedTensor. The tensor shape is not
// filled in this method.
absl::StatusOr<QuantizedTensor> TfTensorToQuantizedTensor(
    const tensorflow::Tensor& tensor) {
  QuantizedTensor quantized;
  switch (tensor.dtype()) {
    case tensorflow::DT_INT8:
      AddValuesToQuantized<int8_t>(&quantized, tensor);
      quantized.bitwidth = 7;
      break;
    case tensorflow::DT_UINT8:
      AddValuesToQuantized<uint8_t>(&quantized, tensor);
      quantized.bitwidth = 8;
      break;
    case tensorflow::DT_INT16:
      AddValuesToQuantized<int16_t>(&quantized, tensor);
      quantized.bitwidth = 15;
      break;
    case tensorflow::DT_UINT16:
      AddValuesToQuantized<uint16_t>(&quantized, tensor);
      quantized.bitwidth = 16;
      break;
    case tensorflow::DT_INT32:
      AddValuesToQuantized<int32_t>(&quantized, tensor);
      quantized.bitwidth = 31;
      break;
    case tensorflow::DT_INT64:
      AddValuesToQuantized<int64_t>(&quantized, tensor);
      quantized.bitwidth = 62;
      break;
    default:
      return absl::InvalidArgumentError(absl::StrCat(
          "Tensor of type", tensorflow::DataType_Name(tensor.dtype()),
          "could not be converted to quantized value"));
  }
  return quantized;
}

absl::StatusOr<absl::flat_hash_map<std::string, QuantizedTensor>>
CreateQuantizedTensorMap(const std::vector<std::string>& tensor_names,
                         const std::vector<tensorflow::Tensor>& tensors,
                         const TensorflowSpec& tensorflow_spec) {
  absl::flat_hash_map<std::string, QuantizedTensor> quantized_tensor_map;
  for (int i = 0; i < tensor_names.size(); i++) {
    absl::StatusOr<QuantizedTensor> quantized =
        TfTensorToQuantizedTensor(tensors[i]);
    if (!quantized.ok()) {
      return quantized.status();
    }
    quantized_tensor_map[tensor_names[i]] = std::move(*quantized);
  }
  // Add dimensions to QuantizedTensors.
  for (const tensorflow::TensorSpecProto& tensor_spec :
       tensorflow_spec.output_tensor_specs()) {
    if (!quantized_tensor_map.contains(tensor_spec.name())) {
      return absl::InvalidArgumentError(absl::StrCat(
          "Tensor spec not found for tensor name: ", tensor_spec.name()));
    }
    auto& quantized = quantized_tensor_map[tensor_spec.name()];
    for (const tensorflow::TensorShapeProto_Dim& dim :
         tensor_spec.shape().dim()) {
      quantized.dimensions.push_back(dim.size());
    }
  }
  return quantized_tensor_map;
}

absl::StatusOr<TaskEligibilityInfo> ParseEligibilityEvalPlanOutput(
    const std::vector<tensorflow::Tensor>& output_tensors) {
  auto output_size = output_tensors.size();
  if (output_size != 1) {
    return absl::InvalidArgumentError(
        absl::StrCat("Unexpected number of output tensors: ", output_size));
  }
  auto output_elements = output_tensors[0].NumElements();
  if (output_elements != 1) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Unexpected number of output tensor elements: ", output_elements));
  }
  tensorflow::DataType output_type = output_tensors[0].dtype();
  if (output_type != tensorflow::DT_STRING) {
    return absl::InvalidArgumentError(
        absl::StrCat("Unexpected output tensor type: ", output_type));
  }

  // Extract the serialized TaskEligibilityInfo proto from the tensor and
  // parse it.
  // First, convert the output Tensor into a Scalar (= a TensorMap with 1
  // element), then use its operator() to access the actual data.
  const tensorflow::tstring& serialized_output =
      output_tensors[0].scalar<const tensorflow::tstring>()();
  TaskEligibilityInfo parsed_output;
  if (!parsed_output.ParseFromString(serialized_output)) {
    return absl::InvalidArgumentError("Could not parse output proto");
  }
  return parsed_output;
}

}  // namespace fcp::client::engine
