/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef FCP_CLIENT_CONVERTERS_H_
#define FCP_CLIENT_CONVERTERS_H_

#include <cstddef>
#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "google/protobuf/repeated_ptr_field.h"
#include "tensorflow_federated/cc/core/impl/aggregation/core/tensor.h"
#include "tensorflow_federated/cc/core/impl/aggregation/core/tensor.pb.h"
#include "tensorflow_federated/cc/core/impl/aggregation/core/tensor_data.h"
#include "tensorflow_federated/cc/core/impl/aggregation/core/tensor_shape.h"

namespace fcp::client {

// A primitive TensorData implementation that holds reference to the repeated
// value field in ExampleQueryResult values.
template <typename T>
class NumericTensorDataAdapter
    : public tensorflow_federated::aggregation::TensorData {
 public:
  explicit NumericTensorDataAdapter(absl::Span<const T> value)
      : value_(value) {}

  size_t byte_size() const override { return value_.size() * sizeof(T); }
  const void* data() const override { return value_.data(); }

 private:
  absl::Span<const T> value_;
};

// Converts repeated numeric field of ExampleQueryResult Values to Aggregation
// Tensor.
template <typename Numeric>
absl::StatusOr<tensorflow_federated::aggregation::Tensor> ConvertNumericTensor(
    tensorflow_federated::aggregation::DataType dtype,
    tensorflow_federated::aggregation::TensorShape tensor_shape,
    absl::Span<const Numeric> value) {
  return tensorflow_federated::aggregation::Tensor::Create(
      dtype, tensor_shape,
      std::make_unique<NumericTensorDataAdapter<Numeric>>(value));
}

// Converts repeated string field of ExampleQueryResult Values to Aggregation
// Tensor.
absl::StatusOr<tensorflow_federated::aggregation::Tensor> ConvertStringTensor(
    tensorflow_federated::aggregation::TensorShape tensor_shape,
    const ::google::protobuf::RepeatedPtrField<std::string>& value);

// Converts a vector of string to Aggregation Tensor.
absl::StatusOr<tensorflow_federated::aggregation::Tensor> ConvertStringTensor(
    const std::vector<std::string>* value);
}  // namespace fcp::client

#endif  // FCP_CLIENT_CONVERTERS_H_
