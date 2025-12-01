// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "fcp/client/converters.h"

#include <string>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "fcp/client/example_query_result.pb.h"
#include "tensorflow_federated/cc/core/impl/aggregation/core/tensor.pb.h"
#include "tensorflow_federated/cc/core/impl/aggregation/core/tensor_shape.h"
#include "tensorflow_federated/cc/core/impl/aggregation/testing/testing.h"

namespace fcp::client {
namespace {

TEST(ConvertersTest, ConvertNumericTensor_Success) {
  ExampleQueryResult::VectorData::Values float_values;
  float_values.mutable_float_values()->add_value(0.24f);
  float_values.mutable_float_values()->add_value(0.42f);
  float_values.mutable_float_values()->add_value(0.33f);

  EXPECT_THAT(*ConvertNumericTensor<float>(
                  tensorflow_federated::aggregation::DT_FLOAT,
                  tensorflow_federated::aggregation::TensorShape({3}),
                  float_values.float_values().value()),
              tensorflow_federated::aggregation::IsTensor<float>(
                  {3}, {0.24f, 0.42f, 0.33f}));
}

TEST(ConvertersTest, ConvertStringTensor_Success) {
  ExampleQueryResult::VectorData::Values string_values;
  string_values.mutable_string_values()->add_value("string_value1");
  string_values.mutable_string_values()->add_value("string_value2");

  EXPECT_THAT(
      *ConvertStringTensor(tensorflow_federated::aggregation::TensorShape({2}),
                           string_values.string_values().value()),
      tensorflow_federated::aggregation::IsTensor<absl::string_view>(
          {2}, {"string_value1", "string_value2"}));
}

TEST(ConvertersTest, ConvertStringTensorFromVector_Success) {
  const std::vector<std::string> string_values = {"string_value1",
                                                  "string_value2"};
  EXPECT_THAT(*ConvertStringTensor(&string_values),
              tensorflow_federated::aggregation::IsTensor<absl::string_view>(
                  {2}, {"string_value1", "string_value2"}));
}
}  // namespace
}  // namespace fcp::client
