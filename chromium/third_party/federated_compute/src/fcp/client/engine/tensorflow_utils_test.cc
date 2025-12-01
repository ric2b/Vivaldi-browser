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
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "fcp/protos/federated_api.pb.h"
#include "fcp/testing/testing.h"
#include "tensorflow/core/framework/tensor.h"
#include "tensorflow/core/framework/tensor_shape.h"
#include "tensorflow/core/platform/tstring.h"

namespace fcp::client::engine {
namespace {

using ::fcp::IsCode;
using ::google::internal::federatedml::v2::TaskEligibilityInfo;

TEST(ParseEligibilityEvalPlanOutputTest, Success) {
  TaskEligibilityInfo expected_output;
  expected_output.set_version(1);
  auto task_weight = expected_output.add_task_weights();
  task_weight->set_task_name("task1");
  task_weight->set_weight(1.0);

  tensorflow::Tensor output_tensor(tensorflow::DT_STRING,
                                   tensorflow::TensorShape({}));
  output_tensor.scalar<tensorflow::tstring>()() =
      expected_output.SerializeAsString();
  std::vector<tensorflow::Tensor> output_tensors = {output_tensor};

  auto actual_output = ParseEligibilityEvalPlanOutput(output_tensors);
  EXPECT_OK(actual_output);
  EXPECT_THAT(*actual_output, EqualsProto(expected_output));
}

TEST(ParseEligibilityEvalPlanOutputTest, UnexpectedNumberOfTensors) {
  TaskEligibilityInfo task_eligibility_info;
  tensorflow::Tensor output_tensor_1(tensorflow::DT_STRING,
                                     tensorflow::TensorShape({}));
  output_tensor_1.scalar<tensorflow::tstring>()() =
      task_eligibility_info.SerializeAsString();
  tensorflow::Tensor output_tensor_2(tensorflow::DT_STRING,
                                     tensorflow::TensorShape({}));
  output_tensor_2.scalar<tensorflow::tstring>()() = "another_string";
  std::vector<tensorflow::Tensor> output_tensors;
  EXPECT_THAT(ParseEligibilityEvalPlanOutput(output_tensors),
              IsCode(absl::StatusCode::kInvalidArgument));
}

TEST(ParseEligibilityEvalPlanOutputTest, UnexpectedNumberOfElements) {
  TaskEligibilityInfo task_eligibility_info;
  tensorflow::Tensor output_tensor(tensorflow::DT_STRING,
                                   tensorflow::TensorShape({2}));
  output_tensor.vec<tensorflow::tstring>()(0) =
      task_eligibility_info.SerializeAsString();
  output_tensor.vec<tensorflow::tstring>()(1) = "another_string";
  std::vector<tensorflow::Tensor> output_tensors = {output_tensor};
  EXPECT_THAT(ParseEligibilityEvalPlanOutput(output_tensors),
              IsCode(absl::StatusCode::kInvalidArgument));
}

TEST(ParseEligibilityEvalPlanOutputTest, UnexpectedTensorType) {
  TaskEligibilityInfo task_eligibility_info;
  tensorflow::Tensor output_tensor(tensorflow::DT_INT32,
                                   tensorflow::TensorShape({}));
  output_tensor.scalar<int32_t>()() = 123;
  std::vector<tensorflow::Tensor> output_tensors = {output_tensor};
  EXPECT_THAT(ParseEligibilityEvalPlanOutput(output_tensors),
              IsCode(absl::StatusCode::kInvalidArgument));
}

TEST(ParseEligibilityEvalPlanOutputTest, UnparseableProto) {
  tensorflow::Tensor output_tensor(tensorflow::DT_STRING,
                                   tensorflow::TensorShape({}));
  output_tensor.scalar<tensorflow::tstring>()() = "not_a_serialized_proto";
  std::vector<tensorflow::Tensor> output_tensors = {output_tensor};
  EXPECT_THAT(ParseEligibilityEvalPlanOutput(output_tensors),
              IsCode(absl::StatusCode::kInvalidArgument));
}
}  // namespace
}  // namespace fcp::client::engine
