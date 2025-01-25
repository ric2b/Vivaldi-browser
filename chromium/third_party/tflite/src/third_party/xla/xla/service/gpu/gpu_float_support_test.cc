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

#include "xla/service/gpu/gpu_float_support.h"

#include <gtest/gtest.h>
#include "absl/status/statusor.h"
#include "xla/hlo/ir/hlo_computation.h"
#include "xla/hlo/ir/hlo_instruction.h"
#include "xla/hlo/ir/hlo_module.h"
#include "xla/hlo/ir/hlo_opcode.h"
#include "xla/service/float_normalization.h"
#include "xla/service/hlo_verifier.h"
#include "xla/shape.h"
#include "xla/shape_util.h"
#include "xla/stream_executor/device_description.h"
#include "xla/test_helpers.h"
#include "xla/tests/hlo_test_base.h"
#include "xla/xla_data.pb.h"

namespace xla::gpu {
namespace {

class FloatSupportTest : public HloTestBase {
 protected:
  FloatSupportTest()
      : HloTestBase(/*verifier_layout_sensitive=*/false,
                    /*allow_mixed_precision_in_hlo_verifier=*/true) {}

  bool Normalize(HloModule* module, se::GpuComputeCapability cc,
                 PrimitiveType low_precision_type,
                 PrimitiveType high_precision_type) {
    GpuFloatSupport float_support(cc, low_precision_type, high_precision_type);
    FloatNormalization normalization(&float_support);
    absl::StatusOr<bool> result = normalization.Run(module);
    EXPECT_IS_OK(result.status());

    HloVerifier verifier(/*layout_sensitive=*/false,
                         /*allow_mixed_precision=*/true);
    EXPECT_IS_OK(verifier.Run(module).status());

    return result.value();
  }

  void TestDotConversion(PrimitiveType lhs_type, PrimitiveType rhs_type,
                         PrimitiveType result_type, se::GpuComputeCapability cc,
                         bool should_convert_lhs, bool should_convert_rhs,
                         PrimitiveType low_precision_type,
                         PrimitiveType high_precision_type = F16) {
    auto builder = HloComputation::Builder(TestName());
    Shape lhs_shape = ShapeUtil::MakeShape(lhs_type, {3, 3});
    Shape rhs_shape = ShapeUtil::MakeShape(rhs_type, {3, 3});
    Shape result_shape = ShapeUtil::MakeShape(result_type, {3, 3});

    HloInstruction* a = builder.AddInstruction(
        HloInstruction::CreateParameter(0, lhs_shape, "a"));
    HloInstruction* b = builder.AddInstruction(
        HloInstruction::CreateParameter(1, rhs_shape, "b"));
    PrecisionConfig precision_config;
    DotDimensionNumbers dot_dnums;
    dot_dnums.add_lhs_contracting_dimensions(1);
    dot_dnums.add_rhs_contracting_dimensions(1);

    builder.AddInstruction(HloInstruction::CreateDot(
        result_shape, a, b, dot_dnums, precision_config));

    auto module = CreateNewVerifiedModule();
    auto computation = module->AddEntryComputation(builder.Build());

    EXPECT_EQ(
        Normalize(module.get(), cc, low_precision_type, high_precision_type),
        should_convert_lhs || should_convert_rhs);

    EXPECT_EQ(computation->root_instruction()->opcode(), HloOpcode::kDot);
    EXPECT_EQ(computation->root_instruction()->operand(0)->opcode() ==
                  HloOpcode::kConvert,
              should_convert_lhs);
    EXPECT_EQ(computation->root_instruction()->operand(1)->opcode() ==
                  HloOpcode::kConvert,
              should_convert_rhs);
  }
};

TEST_F(FloatSupportTest, ShouldAlwaysConvertFp8Dot) {
  TestDotConversion(F8E4M3FN, F8E4M3FN, F16,
                    se::CudaComputeCapability::Hopper(),
                    /*should_convert_lhs=*/true,
                    /*should_convert_rhs=*/true, F8E4M3FN);

  TestDotConversion(F8E4M3FN, F8E4M3FN, F32,
                    se::CudaComputeCapability::Hopper(),
                    /*should_convert_lhs=*/true,
                    /*should_convert_rhs=*/true, F8E4M3FN);

  TestDotConversion(F8E4M3FN, F8E4M3FN, F16,
                    se::CudaComputeCapability::Ampere(),
                    /*should_convert_lhs=*/true,
                    /*should_convert_rhs=*/true, F8E4M3FN);

  TestDotConversion(F8E4M3FN, F8E4M3FN, F32,
                    se::CudaComputeCapability::Hopper(),
                    /*should_convert_lhs=*/true,
                    /*should_convert_rhs=*/true, F8E4M3FN);

  TestDotConversion(F8E5M2, F8E5M2, F16, se::CudaComputeCapability::Ampere(),
                    /*should_convert_lhs=*/true,
                    /*should_convert_rhs=*/true, F8E5M2);

  TestDotConversion(F8E5M2, F8E5M2, F32, se::CudaComputeCapability::Ampere(),
                    /*should_convert_lhs=*/true,
                    /*should_convert_rhs=*/true, F8E5M2);

  TestDotConversion(F8E5M2, F8E4M3FN, F16, se::CudaComputeCapability::Hopper(),
                    /*should_convert_lhs=*/true,
                    /*should_convert_rhs=*/false, F8E5M2);

  TestDotConversion(F8E5M2, F8E4M3FN, F32, se::CudaComputeCapability::Hopper(),
                    /*should_convert_lhs=*/true,
                    /*should_convert_rhs=*/false, F8E5M2);

  TestDotConversion(F8E5M2, F16, F16, se::CudaComputeCapability::Hopper(),
                    /*should_convert_lhs=*/true,
                    /*should_convert_rhs=*/false, F8E5M2);

  TestDotConversion(F8E5M2, F16, F32, se::CudaComputeCapability::Hopper(),
                    /*should_convert_lhs=*/true,
                    /*should_convert_rhs=*/false, F8E5M2);
}

TEST_F(FloatSupportTest, ShouldKeepBf16OnAmpere) {
  TestDotConversion(BF16, BF16, F32, se::CudaComputeCapability::Ampere(),
                    /*should_convert_lhs=*/false,
                    /*should_convert_rhs=*/false, BF16);
}

TEST_F(FloatSupportTest, ShouldKeepBf16OnHopper) {
  TestDotConversion(BF16, BF16, F32, se::CudaComputeCapability::Hopper(),
                    /*should_convert_lhs=*/false,
                    /*should_convert_rhs=*/false, BF16);
}

}  // namespace
}  // namespace xla::gpu
