/*
 * Copyright 2019 Google LLC
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

/**
 * Runs a trivial TensorFlow session - just to know we can actually build it
 * correctly.
 */

#include <stdint.h>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "tensorflow/cc/client/client_session.h"
#include "tensorflow/cc/ops/standard_ops.h"
#include "tensorflow/core/framework/tensor.h"
#include "tensorflow/core/framework/tensor_testutil.h"

namespace {

using ::testing::Eq;

using tensorflow::ClientSession;
using tensorflow::Scope;
using tensorflow::Tensor;
using tensorflow::TensorShape;
using tensorflow::test::AsTensor;
using tensorflow::test::ExpectTensorEqual;
using tensorflow::ops::Const;
using tensorflow::ops::Mul;

TEST(TfSmokeTest, DoTrickyMath) {
  Scope root = Scope::NewRootScope();
  auto a = Const<int32_t>(root, { {1, 2}, {3, 4} });
  auto b = Const<int32_t>(root, { {2} });
  auto r = Mul(root.WithOpName("r"), a, b);
  std::vector<Tensor> outputs;

  ClientSession session(root);
  TF_CHECK_OK(session.Run({r}, &outputs));

  Tensor expected = AsTensor<int32_t>(
      {2, 4, 6, 8},
      TensorShape({2, 2}));

  EXPECT_THAT(outputs.size(), Eq(1));
  ExpectTensorEqual<int32_t>(outputs[0], expected);
}

}  // namespace
