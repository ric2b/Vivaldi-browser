/* Copyright 2018 The OpenXLA Authors.

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

#include <memory>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "xla/error_spec.h"
#include "xla/service/gpu/tests/gpu_codegen_test.h"
#include "xla/service/hlo_module_config.h"
#include "xla/tests/hlo_test_base.h"
#include "xla/tests/verified_hlo_module.h"
#include "tsl/platform/test.h"

namespace xla {
namespace gpu {
namespace {

class GpuKernelTilingTest : public GpuCodegenTest {
 protected:
  // Most tests in this file want to skip layout assignment, but a few need it
  // enabled.
  HloModuleConfig ConfigWithLayoutAssignment() {
    HloModuleConfig config;
    auto debug_options = HloTestBase::GetDebugOptionsForTest();
    debug_options.set_xla_gpu_mlir_emitter_level(3);
    config.set_debug_options(debug_options);
    return config;
  }

  HloModuleConfig ConfigWithoutLayoutAssignment() {
    HloModuleConfig config;
    auto debug_options = HloTestBase::GetDebugOptionsForTest();
    // Disable layout_assignment to use the preassigned layouts.
    debug_options.add_xla_disable_hlo_passes("layout-assignment");
    debug_options.set_xla_gpu_mlir_emitter_level(3);
    config.set_debug_options(debug_options);
    return config;
  }
};

TEST_F(GpuKernelTilingTest, UnnestedTransposeWithProperDimensionsTiled) {
  const char *const kHloString = R"(
    HloModule unnested_transpose_1

    ENTRY unnested_transpose_1 {
      para0 = f16[48,64]{1,0} parameter(0)
      ROOT t = f16[64,48]{1,0} transpose(para0), dimensions={1,0}
    })";

  // Check that a call to llvm.nvvm.barrier0 is generated.
  //
  // We must enable layout assignment in order for this test to work correctly.
  // AlgebraicSimplifier removes 't'; it's added back by layout assignment,
  // which respects the module's entry computation layout.  But if we don't run
  // layout assignment...well, nobody else adds the transpose back.
  auto hlo_module =
      ParseAndReturnVerifiedModule(kHloString, ConfigWithLayoutAssignment())
          .value();

  auto expected_ir = R"(
; CHECK: call void BARRIER()
)";
  CompileAndVerifyIr(std::move(hlo_module),
                     MakePlatformSpecificLlvm(expected_ir),
                     /*match_optimized_ir=*/true);

  // Check that the kernel runs correctly.
  EXPECT_TRUE(RunAndCompareNoHloPasses(kHloString, ErrorSpec{0.0}));
}

TEST_F(GpuKernelTilingTest, UnnestedTransposeWithSmallDimensionsNotTiled) {
  const char *const kHloString = R"(
    HloModule unnested_transpose_2

    ENTRY unnested_transpose_2 {
      para0 = f16[6,4]{1,0} parameter(0)
      ROOT t = f16[4,6]{1,0} transpose(para0), dimensions={1,0}
    })";

  // Check that a call to llvm.nvvm.barrier0 is not generated.  As in
  // UnnestedTransposeWithProperDimensionsTiled, we must run layout assignment
  // here.
  auto hlo_module =
      ParseAndReturnVerifiedModule(kHloString, ConfigWithLayoutAssignment())
          .value();
  auto expected_ir = R"(
; CHECK-NOT: call void BARRIER()
)";
  CompileAndVerifyIr(std::move(hlo_module),
                     MakePlatformSpecificLlvm(expected_ir),
                     /*match_optimized_ir=*/true);
}

TEST_F(GpuKernelTilingTest, UnnestedTransposeC128TypeRun) {
  const char *const kHloString = R"(
    HloModule unnested_transpose_3

    ENTRY unnested_transpose_3 {
      para0 = c128[65,65]{1,0} parameter(0)
      ROOT t = c128[65,65]{1,0} transpose(para0), dimensions={1,0}
    })";

  auto hlo_module =
      ParseAndReturnVerifiedModule(kHloString, ConfigWithLayoutAssignment())
          .value();
  auto expected_ir = R"(
; CHECK: call void BARRIER()
)";
  CompileAndVerifyIr(std::move(hlo_module),
                     MakePlatformSpecificLlvm(expected_ir),
                     /*match_optimized_ir=*/true);

  EXPECT_TRUE(RunAndCompareNoHloPasses(kHloString, ErrorSpec{0.0}));
}

TEST_F(GpuKernelTilingTest, SimpleFusionWithTransposeTiled) {
  const char *const kHloString = R"(
    HloModule multiple_output_fusion_1
    fused_computation.1 {
      param0 = f32[4,30,56]{2,1,0} parameter(0)
      convert = f16[4,30,56]{2,1,0} convert(param0)
      ROOT t = f16[4,56,30]{2,1,0} transpose(convert), dimensions={0,2,1}
    }

    ENTRY copy_in_fusion_run_without_hlo_passes {
      para0 = f32[4,30,56]{2,1,0} parameter(0)
      ROOT fusion.1 = f16[4,56,30]{2,1,0} fusion(para0), kind=kLoop,
        calls=fused_computation.1
    })";

  // Check that a call to llvm.nvvm.barrier0 is generated.
  auto hlo_module =
      ParseAndReturnVerifiedModule(kHloString, ConfigWithoutLayoutAssignment())
          .value();
  auto expected_ir = R"(
; CHECK-LABEL: define KERNEL_ANNOTATION @{{[a-z_]*}}fusion
; CHECK: call void BARRIER()
; CHECK: }
)";
  CompileAndVerifyIr(std::move(hlo_module),
                     MakePlatformSpecificLlvm(expected_ir),
                     /*match_optimized_ir=*/true);

  // Check that the kernel runs correctly.
  EXPECT_TRUE(RunAndCompareNoHloPasses(kHloString, ErrorSpec{0.0}));
}

TEST_F(GpuKernelTilingTest, MultipleOutputFusionWithOnePossibleTransposeTiled) {
  const char *const kHloString = R"(
    HloModule multiple_output_fusion_1
    fused_computation.1 {
      param0 = f16[8,961,65]{2,1,0} parameter(0)
      param1 = f16[8,961,65]{2,1,0} parameter(1)
      t0 = f16[8,65,961]{2,1,0} transpose(param0),dimensions={0,2,1}
      t1 = f16[8,65,961]{2,1,0} transpose(param1), dimensions={0,2,1}
      ROOT tuple1 = (f16[8,65,961]{2,1,0}, f16[8,65,961]{2,1,0})
        tuple(t0, t1)
    }

    ENTRY multiple_output_fusion_1 {
      para0 = f16[8,961,65]{2,1,0} parameter(0)
      para1 = f16[8,961,65]{2,1,0} parameter(1)
      ROOT fusion.1 = (f16[8,65,961]{2,1,0}, f16[8,65,961]{2,1,0})
        fusion(para0,para1), kind=kInput, calls=fused_computation.1
    })";

  // Check that a call to llvm.nvvm.barrier0 is generated.
  auto hlo_module =
      ParseAndReturnVerifiedModule(kHloString, ConfigWithoutLayoutAssignment())
          .value();
  auto expected_ir = R"(
; CHECK-LABEL: define KERNEL_ANNOTATION @{{[a-z_]*}}fusion
; CHECK: call void BARRIER()
; CHECK: }
)";
  CompileAndVerifyIr(std::move(hlo_module),
                     MakePlatformSpecificLlvm(expected_ir),
                     /*match_optimized_ir=*/true);

  // Check that the kernel runs correctly.
  EXPECT_TRUE(RunAndCompareNoHloPasses(kHloString, ErrorSpec{0.0}));
}

TEST_F(GpuKernelTilingTest, TransposedInputWithUserReverseNotTiled) {
  const char *const kHloString = R"(
    HloModule FusionTransposeWithReverseNotTiled
    fused_computation.1 {
      arg0 = f32[128,64]{1,0} parameter(0)
      t = f32[64,128]{1,0} transpose(arg0), dimensions={1,0}
      ROOT reverse0 = f32[64,128]{1,0} reverse(t), dimensions={0}
    }

    ENTRY reverse_break_assumption {
      param0 = f32[128,64]{1,0} parameter(0)
      ROOT fusion0 = f32[64,128]{1,0} fusion(param0), kind=kLoop,
        calls=fused_computation.1
    })";

  // Check that a call to llvm.nvvm.barrier0 is not generated.
  auto hlo_module =
      ParseAndReturnVerifiedModule(kHloString, ConfigWithoutLayoutAssignment())
          .value();
  auto expected_ir = R"(
; CHECK-LABEL: define KERNEL_ANNOTATION @{{[a-z_]*}}fusion
; CHECK-NOT: call void BARRIER()
; CHECK: }
)";
  CompileAndVerifyIr(std::move(hlo_module),
                     MakePlatformSpecificLlvm(expected_ir),
                     /*match_optimized_ir=*/true);
}

TEST_F(GpuKernelTilingTest, TransposedInputWithUserBitcastNotTiled) {
  const char *const kHloString = R"(
    HloModule TransposedInputWithUserBitcast

    fused_computation {
      param_0 = f32[20,20]{1,0} parameter(0)
      ROOT bitcast = f32[20,20]{0,1} bitcast(param_0)
    }

    ENTRY kernel_entry {
      parameter.0 = f32[20,20]{1,0} parameter(0)
      ROOT fusion = f32[20,20]{0,1} fusion(parameter.0),
        kind=kLoop, calls=fused_computation
    })";

  // Check that a call to llvm.nvvm.barrier0 is not generated.
  auto hlo_module =
      ParseAndReturnVerifiedModule(kHloString, ConfigWithoutLayoutAssignment())
          .value();
  auto expected_ir = R"(
; CHECK-LABEL: define KERNEL_ANNOTATION @{{[a-z_]*}}fusion
; CHECK-NOT: call void BARRIER()
; CHECK: }
)";
  CompileAndVerifyIr(std::move(hlo_module),
                     MakePlatformSpecificLlvm(expected_ir),
                     /*match_optimized_ir=*/true);

  // Check that the kernel runs correctly.
  EXPECT_TRUE(RunAndCompareNoHloPasses(kHloString, ErrorSpec{0.0}));
}

TEST_F(GpuKernelTilingTest, TransposedInputWithoutUnsafeUseTiled) {
  const char *const kHloString = R"(
    HloModule TwoTransposedInputs

    fused_computation {
      param_0 = f32[16,16]{1,0} parameter(0)
      param_1 = f32[16,16]{1,0} parameter(1)
      s = f32[16,16]{1,0} exponential(param_0)
      t = f32[16,16]{1,0} transpose(param_1), dimensions={1,0}
      ROOT tuple = (f32[16,16]{1,0}, f32[16,16]{1,0}) tuple(s, t)
    }

    ENTRY kernel_entry {
      parameter.0 = f32[16,16]{1,0} parameter(0)
      parameter.1 = f32[16,16]{1,0} parameter(1)
      ROOT fusion = (f32[16,16]{1,0}, f32[16,16]{1,0})
        fusion(parameter.0, parameter.1),
        kind=kInput, calls=fused_computation
    })";

  // Check that a call to llvm.nvvm.barrier0 is generated.
  auto hlo_module =
      ParseAndReturnVerifiedModule(kHloString, ConfigWithoutLayoutAssignment())
          .value();
  auto expected_ir = R"(
; CHECK-LABEL: define KERNEL_ANNOTATION @{{[a-z_]*}}fusion
; CHECK: call void BARRIER()
; CHECK: }
)";
  CompileAndVerifyIr(std::move(hlo_module),
                     MakePlatformSpecificLlvm(expected_ir),
                     /*match_optimized_ir=*/true);
  // Check that the kernel runs correctly.
  EXPECT_TRUE(RunAndCompareNoHloPasses(kHloString, ErrorSpec{0.0001}));
}

TEST_F(GpuKernelTilingTest, MofReduceDifferentType) {
  const char *const kHloString = R"(
HloModule module, entry_computation_layout={(f32[128,1024]{1,0})->(f16[128]{0}, f32[128]{0})}

scalar_add_computation_f16 {
  scalar_lhs.0 = f16[] parameter(0)
  scalar_rhs.0 = f16[] parameter(1)
  ROOT add.0 = f16[] add(scalar_lhs.0, scalar_rhs.0)
}

scalar_add_computation {
  scalar_lhs.1 = f32[] parameter(0)
  scalar_rhs.1 = f32[] parameter(1)
  ROOT add.1 = f32[] add(scalar_lhs.1, scalar_rhs.1)
}

fused_computation {
  param_0.2 = f32[128,1024]{1,0} parameter(0)
  p16.1 = f16[128,1024]{1,0} convert(param_0.2)
  c16_1 = f16[] constant(0)
  r0.1 = f16[128]{0} reduce(p16.1, c16_1), dimensions={1}, to_apply=scalar_add_computation_f16
  c32_1 = f32[] constant(0)
  r1.1 = f32[128]{0} reduce(param_0.2, c32_1), dimensions={1}, to_apply=scalar_add_computation
  ROOT tuple = (f16[128]{0}, f32[128]{0}) tuple(r0.1, r1.1)
}

ENTRY entry {
  p = f32[128,1024]{1,0} parameter(0)
  ROOT fusion = (f16[128]{0}, f32[128]{0}) fusion(p), kind=kInput, calls=fused_computation
})";
  EXPECT_TRUE(RunAndCompareNoHloPasses(kHloString, ErrorSpec{1.0e-3, 1.0e-3}));
}

TEST_F(GpuKernelTilingTest, ColumnReductionWithLayoutChangeTiled) {
  const char *const kHloString = R"(
    HloModule reduce_with_layout_change
    reduction0 {
      x0 = f32[] parameter(0)
      y0 = f32[] parameter(1)
      ROOT add0 = f32[] add(x0, y0)
    }

    ENTRY kernel_entry {
      arg0 = f32[4,32,32,16,12,12,3,3]{2,3,5,4,0,7,6,1}  parameter(0)
      constant0 = f32[] constant(0)
      ROOT reduce0 = f32[4,32,16,12,12]{4,3,2,1,0} reduce(arg0, constant0),
        dimensions={1,6,7}, to_apply=reduction0
    })";

  // Check that the kernel is tiled by looking for llvm.nvvm.atomic.
  auto hlo_module =
      ParseAndReturnVerifiedModule(kHloString, ConfigWithoutLayoutAssignment())
          .value();
  const char *expected_ir = R"(
; CHECK-LABEL: define KERNEL_ANNOTATION @
; CHECK: store float %{{.*}}, ptr addrspace(1)
; CHECK: }
)";
  CompileAndVerifyIr(std::move(hlo_module),
                     MakePlatformSpecificLlvm(expected_ir),
                     /*match_optimized_ir=*/true);

  // Check that the kernel runs correctly.
  EXPECT_TRUE(RunAndCompare(kHloString, ErrorSpec{0.001}));
}

TEST_F(GpuKernelTilingTest, RowReductionWithLayoutChangeTiled) {
  const char *const kHloString = R"(
    HloModule reduce_with_layout_change
    reduction0 {
      x0 = f32[] parameter(0)
      y0 = f32[] parameter(1)
      ROOT add0 = f32[] add(x0, y0)
    }

    ENTRY kernel_entry {
      arg0 = f32[8,6,64]{2,1,0}  parameter(0)
      constant0 = f32[] constant(0)
      ROOT reduce0 = f32[8,6]{0,1} reduce(arg0, constant0), dimensions={2},
        to_apply=reduction0
    })";

  // Check that the kernel is tiled by looking for llvm.nvvm.shfl.sync.down.
  auto hlo_module =
      ParseAndReturnVerifiedModule(kHloString, ConfigWithoutLayoutAssignment())
          .value();
  auto expected_ir = R"(
; CHECK-LABEL: define KERNEL_ANNOTATION @{{(wrapped_reduce|.*fusion)}}
; CHECK: call SHUFFLE
; CHECK: }
)";
  CompileAndVerifyIr(std::move(hlo_module),
                     MakePlatformSpecificLlvm(expected_ir),
                     /*match_optimized_ir=*/true);

  // Check that the kernel runs correctly.
  EXPECT_TRUE(RunAndCompare(kHloString, ErrorSpec{0.001}));
}

TEST_F(GpuKernelTilingTest, RowReductionTwoRowsPerWarp) {
  const char *const kHloString = R"(
    HloModule reduce_with_layout_change
    reduction0 {
      x0 = f32[] parameter(0)
      y0 = f32[] parameter(1)
      ROOT add0 = f32[] add(x0, y0)
    }

    ENTRY kernel_entry {
      arg0 = f32[10000,16]{1,0}  parameter(0)
      constant0 = f32[] constant(0)
      ROOT reduce0 = f32[10000]{0} reduce(arg0, constant0), dimensions={1},
        to_apply=reduction0
    })";

  // Check that the kernel is tiled by looking for llvm.nvvm.shfl.sync.down and
  // a write condition based on the logical thread ID (two writes per warp).
  auto hlo_module =
      ParseAndReturnVerifiedModule(kHloString, ConfigWithoutLayoutAssignment())
          .value();
  auto expected_ir = R"(
; CHECK-LABEL: define KERNEL_ANNOTATION @{{(wrapped_reduce|.*fusion)}}
; CHECK: %[[TID_X:.*]] = tail call i32 TIDX()
; CHECK: %[[TID_LOGICAL:.*]] = and i32 %[[TID_X]], 15
; CHECK: call SHUFFLE
; CHECK: %[[LOGICAL_T0:.*]] = icmp eq i32 %[[TID_LOGICAL]], 0
; CHECK: LCAL
; CHECK: EXTV
; CHECK: BR_CAL
)";
  CompileAndVerifyIr(std::move(hlo_module),
                     MakePlatformSpecificLlvm(expected_ir),
                     /*match_optimized_ir=*/true);

  // Check that the kernel runs correctly.
  EXPECT_TRUE(RunAndCompareNoHloPasses(kHloString, ErrorSpec{0.001}));
}

TEST_F(GpuKernelTilingTest, RowReductionFourRowsPerWarp) {
  const char *const kHloString = R"(
    HloModule reduce_with_layout_change
    reduction0 {
      x0 = f32[] parameter(0)
      y0 = f32[] parameter(1)
      ROOT add0 = f32[] add(x0, y0)
    }

    ENTRY kernel_entry {
      arg0 = f32[10000,8]{1,0}  parameter(0)
      constant0 = f32[] constant(0)
      ROOT reduce0 = f32[10000]{0} reduce(arg0, constant0), dimensions={1},
        to_apply=reduction0
    })";

  // Check that the kernel is tiled by looking for llvm.nvvm.shfl.sync.down and
  // a write condition based on the logical thread ID (four writes per warp).
  auto hlo_module =
      ParseAndReturnVerifiedModule(kHloString, ConfigWithoutLayoutAssignment())
          .value();
  auto expected_ir = R"(
; CHECK-LABEL: define KERNEL_ANNOTATION @{{(wrapped_reduce|.*fusion)}}
; CHECK: %[[TID_X:.*]] = tail call i32 TIDX()
; CHECK: %[[TID_LOGICAL:.*]] = and i32 %[[TID_X]], 7
; CHECK: call SHUFFLE
; CHECK: %[[LOGICAL_T0:.*]] = icmp eq i32 %[[TID_LOGICAL]], 0
)";

  CompileAndVerifyIr(std::move(hlo_module),
                     MakePlatformSpecificLlvm(expected_ir),
                     /*match_optimized_ir=*/true);

  // Check that the kernel runs correctly.
  EXPECT_TRUE(RunAndCompareNoHloPasses(kHloString, ErrorSpec{0.001}));
}

TEST_F(GpuKernelTilingTest,
       ColumnReductionResultTwoPartsWithLayoutChangeTiled) {
  const char *const kHloString = R"(
    HloModule reduce_with_no_layout_change
    reduction0 {
      x0 = f32[] parameter(0)
      y0 = f32[] parameter(1)
      ROOT add0 = f32[] add(x0, y0)
    }

    ENTRY kernel_entry {
      arg0 = f32[8,64,32]{2,1,0}  parameter(0)
      constant0 = f32[] constant(0)
      ROOT reduce0 = f32[8,32]{0,1} reduce(arg0, constant0), dimensions={1},
        to_apply=reduction0
    })";

  // Check that the kernel is tiled by looking for llvm.nvvm.atomic.
  auto hlo_module =
      ParseAndReturnVerifiedModule(kHloString, ConfigWithoutLayoutAssignment())
          .value();
  const char *expected_ir = R"(
; CHECK-LABEL: define KERNEL_ANNOTATION @{{(wrapped_reduce|.*fusion)}}
; CHECK: store float %{{.*}}, ptr addrspace(1)
; CHECK: }
)";
  CompileAndVerifyIr(std::move(hlo_module),
                     MakePlatformSpecificLlvm(expected_ir),
                     /*match_optimized_ir=*/true);

  // Check that the kernel runs correctly.
  EXPECT_TRUE(RunAndCompareNoHloPasses(kHloString, ErrorSpec{0.001}));
}

TEST_F(GpuKernelTilingTest, ColumnReductionSmallTileSizeX) {
  const char *const kHloString = R"(
  HloModule Test

  scalar_add_computation.1 {
    scalar_lhs.1 = f32[] parameter(0)
    scalar_rhs.1 = f32[] parameter(1)
    ROOT add.6 = f32[] add(scalar_lhs.1, scalar_rhs.1)
  }
  ENTRY Test {
    param_3.241 = f16[512,2,9,9]{1,3,2,0} parameter(3)
    constant_661 = f16[] constant(0)
    broadcast.695 = f16[512,2,9,9]{1,3,2,0} broadcast(constant_661), dimensions={}
    compare.42 = pred[512,2,9,9]{1,3,2,0} compare(param_3.241, broadcast.695), direction=GT
    param_2.401 = f16[512,2,9,9]{1,3,2,0} parameter(2)
    select.40 = f16[512,2,9,9]{1,3,2,0} select(compare.42, param_2.401, broadcast.695)
    convert.196 = f32[512,2,9,9]{1,3,2,0} convert(select.40)
    param_1.809 = f16[512,2,9,9]{1,3,2,0} parameter(1)
    copy.335 = f16[512,2,9,9]{1,3,2,0} copy(param_1.809)
    convert.218 = f32[512,2,9,9]{1,3,2,0} convert(copy.335)
    param_0.668 = f32[2]{0} parameter(0)
    broadcast.687 = f32[512,2,9,9]{1,3,2,0} broadcast(param_0.668), dimensions={1}
    subtract.136 = f32[512,2,9,9]{1,3,2,0} subtract(convert.218, broadcast.687)
    multiply.579 = f32[512,2,9,9]{1,3,2,0} multiply(convert.196, subtract.136)
    constant_485 = f32[] constant(0)
    reduce.139 = f32[2]{0} reduce(multiply.579, constant_485), dimensions={0,2,3}, to_apply=scalar_add_computation.1
    reduce.140.clone.1 = f32[2]{0} reduce(convert.196, constant_485), dimensions={0,2,3}, to_apply=scalar_add_computation.1
    ROOT tuple.102 = (f32[2]{0}, f32[2]{0}) tuple(reduce.139, reduce.140.clone.1)
  })";

  // Check that no loop is generated for reduction.
  auto hlo_module =
      ParseAndReturnVerifiedModule(kHloString, ConfigWithoutLayoutAssignment())
          .value();
  const char *expected_ir = R"(
; CHECK-NOT: reduce.0.loop_header
; CHECK: }
)";
  CompileAndVerifyIr(std::move(hlo_module), expected_ir,
                     /*match_optimized_ir=*/true);
  // Check that the kernel runs correctly.
  EXPECT_TRUE(RunAndCompare(kHloString, ErrorSpec{1.0e-5, 1.0e-5}));
}

TEST_F(GpuKernelTilingTest,
       RowReductionWithSmallNonPowerOfTwoDimensionNotTiled) {
  const char *const kHloString = R"(
    HloModule reduction
    reduction0 {
      x0 = f32[] parameter(0)
      y0 = f32[] parameter(1)
      ROOT add0 = f32[] add(x0, y0)
    }

    ENTRY kernel_entry {
      arg0 = f32[8,6,15]{2,1,0}  parameter(0)
      constant0 = f32[] constant(0)
      ROOT reduce0 = f32[8,6]{1,0} reduce(arg0, constant0), dimensions={2},
        to_apply=reduction0
    })";

  // Check that the kernel is not tiled by looking for llvm.nvvm.shfl.sync.down.
  auto hlo_module =
      ParseAndReturnVerifiedModule(kHloString, ConfigWithoutLayoutAssignment())
          .value();
  auto expected_ir = R"(
; CHECK-LABEL: define KERNEL_ANNOTATION @{{(wrapped_reduce|.*fusion)}}
; CHECK-NOT: call SHUFFLE
; CHECK: }
)";
  CompileAndVerifyIr(std::move(hlo_module),
                     MakePlatformSpecificLlvm(expected_ir),
                     /*match_optimized_ir=*/true);

  // Check that the kernel runs correctly.
  EXPECT_TRUE(RunAndCompareNoHloPasses(kHloString, ErrorSpec{0.001}));
}

TEST_F(GpuKernelTilingTest, RowReductionRequiring64BitIndex) {
  const char *const kHloString = R"(
  HloModule LargeReduction

  Sum {
    x.1 = f32[] parameter(0)
    y.1 = f32[] parameter(1)
    ROOT add.1 = f32[] add(x.1, y.1)
  }

  ENTRY reduce.1 {
    parameter = f32[3048576000] parameter(0)
    init_value = f32[] constant(0)
    ROOT out = f32[] reduce(parameter, init_value), dimensions={0}, to_apply=Sum
  }
  )";
  std::unique_ptr<VerifiedHloModule> hlo_module =
      ParseAndReturnVerifiedModule(kHloString).value();
  const char *expected_ir = R"(
; CHECK: i64
  )";
  CompileAndVerifyIr(std::move(hlo_module), expected_ir,
                     /*match_optimized_ir=*/true);
}

TEST_F(GpuKernelTilingTest, Hlo021CopyNoOobAccess) {
  const char *const kHloString = R"(
HloModule primitive_computation_svd.38

%fused_computation (param_0.7: f32[841,3], param_1.10: pred[3]) -> f32[3,841] {
  %param_1.10 = pred[3]{0} parameter(1)
  %broadcast.7 = pred[3,841]{1,0} broadcast(pred[3]{0} %param_1.10), dimensions={0}
  %param_0.7 = f32[841,3]{1,0} parameter(0)
  %transpose = f32[3,841]{1,0} transpose(f32[841,3]{1,0} %param_0.7), dimensions={1,0}
  %constant_1 = f32[] constant(nan)
  %broadcast.6 = f32[3,841]{1,0} broadcast(f32[] %constant_1), dimensions={}
  ROOT %select.0 = f32[3,841]{1,0} select(pred[3,841]{1,0} %broadcast.7, f32[3,841]{1,0} %transpose, f32[3,841]{1,0} %broadcast.6)
}

ENTRY %primitive_computation_svd.38 (constant_5: f32[841,3], fusion.3: pred[3]) -> f32[3,841] {
  %constant_5 = f32[841,3]{1,0} parameter(0)
  %fusion.3 = pred[3]{0} parameter(1)
  ROOT %fusion = f32[3,841]{1,0} fusion(f32[841,3]{1,0} %constant_5, pred[3]{0} %fusion.3), kind=kLoop, calls=%fused_computation
}
  )";

  // Test against the OOB read due to a ptxas bug.
  EXPECT_TRUE(RunAndCompareNoHloPasses(kHloString, ErrorSpec{0.001}));
}

TEST_F(GpuKernelTilingTest, RowReductionCorrectShmemUsage) {
  const char *const kHloString = R"(
  HloModule RowReduce

  Sum {
    x.1 = f32[] parameter(0)
    y.1 = f32[] parameter(1)
    ROOT add.1 = f32[] add(x.1, y.1)
  }

  ENTRY reduce.1 {
    parameter = f32[1048576] parameter(0)
    init_value = f32[] constant(0)
    ROOT reduce = f32[] reduce(parameter, init_value), dimensions={0}, to_apply=Sum
  }
  )";
  auto hlo_module = ParseAndReturnVerifiedModule(kHloString).value();
  auto &debug_options = hlo_module->mutable_config().mutable_debug_options();
  debug_options.set_xla_gpu_mlir_emitter_level(3);
  auto expected_ir = is_built_with_rocm_ ? R"(
; CHECK: %llvm.amdgcn.kernel.input_reduce_fusion.lds.t = type { [4 x [2 x float]] }
; CHECK: @llvm.amdgcn.kernel.input_reduce_fusion.lds = internal addrspace(3) global %llvm.amdgcn.kernel.input_reduce_fusion.lds.t poison
  )"
                                         : R"(
; CHECK: shared_cache = private unnamed_addr addrspace({{[0-9]*}}) global [4 x [2 x float]]
  )";
  CompileAndVerifyIr(std::move(hlo_module), expected_ir,
                     /*match_optimized_ir=*/true);
}

TEST_F(GpuKernelTilingTest, ReductionInputTooLarge) {
  const char *const kHloString = R"(
  HloModule RowReduce

  Sum {
    x.1 = f32[] parameter(0)
    y.1 = f32[] parameter(1)
    ROOT add.1 = f32[] add(x.1, y.1)
  }

  ENTRY reduce.1 {
    parameter = f32[16,1048576,1024,1024] parameter(0)
    init_value = f32[] constant(0)
    ROOT reduce = f32[16,1048576,1024] reduce(parameter, init_value), dimensions={3}, to_apply=Sum
  }
  )";
  auto hlo_module = ParseAndReturnVerifiedModule(kHloString).value();
  absl::Status status = CompileToExecutable(std::move(hlo_module)).status();
  EXPECT_THAT(status.message(),
              ::testing::ContainsRegex(
                  "Kernel '.*' launch needs more blocks [(]4294967296[)] than "
                  "allowed by hardware [(]2147483647[)]"));
}

}  // namespace
}  // namespace gpu
}  // namespace xla
