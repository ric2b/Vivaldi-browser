/* Copyright 2024 The OpenXLA Authors.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include "xla/service/gpu/fusions/ir/xla_gpu_ops.h"

#include <gtest/gtest.h>
#include "xla/service/gpu/model/indexing_map.h"
#include "xla/service/gpu/model/indexing_map_serialization.h"
#include "xla/service/gpu/model/indexing_test_utils.h"
#include "xla/tests/hlo_test_base.h"
#include "tsl/platform/test.h"

namespace xla::gpu {
namespace {

using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

class XLAGPUOpsTest : public HloTestBase {
 public:
  mlir::MLIRContext mlir_context_;
};

TEST_F(XLAGPUOpsTest, GetConstraintsForVariables) {
  auto map = *ParseIndexingMap(R"(
    (d0, d1)[s0, s1] -> (d0 + s0, d1 + s1),
    domain: d0 in [0, 5],
    d1 in [0, 2],
    s0 in [0, 32],
    s1 in [0, 1024],
    d1 + s1 in [0, 4],
    d1 mod 32 in [0, 6],
    s0 + s1 in [0, 3],
    s0 mod 4 in [0, 1],
    s1 mod 4 in [0, 2]
  )",
                               &mlir_context_);
  auto constraints_for_variables = GetConstraintsForVariables(map);
  EXPECT_THAT(constraints_for_variables.constraints_for_dims[0],
              UnorderedElementsAre());
  EXPECT_THAT(
      constraints_for_variables.constraints_for_dims[1],
      UnorderedElementsAre(
          Pair(ParseAffineExpr("s1 + d1", &mlir_context_), Interval{0, 4}),
          Pair(ParseAffineExpr("d1 mod 32", &mlir_context_), Interval{0, 6})));
  EXPECT_THAT(
      constraints_for_variables.constraints_for_symbols[0],
      UnorderedElementsAre(
          Pair(ParseAffineExpr("s0 mod 4", &mlir_context_), Interval{0, 1}),
          Pair(ParseAffineExpr("s0 + s1", &mlir_context_), Interval{0, 3})));
  EXPECT_THAT(
      constraints_for_variables.constraints_for_symbols[1],
      UnorderedElementsAre(
          Pair(ParseAffineExpr("s1 mod 4", &mlir_context_), Interval{0, 2}),
          Pair(ParseAffineExpr("s0 + s1", &mlir_context_), Interval{0, 3}),
          Pair(ParseAffineExpr("s1 + d1", &mlir_context_), Interval{0, 4})));
}

TEST_F(XLAGPUOpsTest, GetConstraintsForVariablesEmpty) {
  auto map = *ParseIndexingMap(R"(
    (d0, d1)[s0, s1] -> (d0 + s0, d1 + s1),
    domain: d0 in [0, 5],
    d1 in [0, 2],
    s0 in [0, 32],
    s1 in [0, 1024],
  )",
                               &mlir_context_);
  auto constraints_for_variables = GetConstraintsForVariables(map);
  EXPECT_THAT(constraints_for_variables.constraints_for_dims,
              ElementsAre(IsEmpty(), IsEmpty()));
  EXPECT_THAT(constraints_for_variables.constraints_for_symbols,
              ElementsAre(IsEmpty(), IsEmpty()));
}

}  // namespace
}  // namespace xla::gpu
