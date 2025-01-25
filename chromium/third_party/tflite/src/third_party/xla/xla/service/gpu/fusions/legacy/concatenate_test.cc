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
#include "xla/service/gpu/fusions/legacy/concatenate.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "mlir/IR/MLIRContext.h"
#include "mlir/Support/LLVM.h"
#include "xla/service/gpu/fusions/fusions.h"
#include "xla/service/gpu/gpu_device_info_for_tests.h"
#include "xla/service/gpu/hlo_fusion_analysis.h"
#include "xla/service/gpu/model/indexing_map_serialization.h"
#include "xla/service/gpu/model/indexing_test_utils.h"
#include "xla/stream_executor/device_description.h"
#include "xla/tests/hlo_test_base.h"
#include "xla/xla.pb.h"

namespace xla {
namespace gpu {
namespace {

class ConcatenateTest : public HloTestBase {
 protected:
  DebugOptions GetDebugOptionsForTest() const override {
    auto opts = HloTestBase::GetDebugOptionsForTest();
    opts.set_xla_gpu_mlir_emitter_level(0);
    return opts;
  }
  mlir::MLIRContext mlir_context_;
};

TEST_F(ConcatenateTest, ThreadIndexing) {
  auto module = ParseAndReturnVerifiedModule(R"(
    HloModule module

    fused_computation {
      param0 = f32[200] parameter(0)
      param1 = f32[400] parameter(1)
      param2 = f32[300] parameter(2)
      ROOT concat = f32[900] concatenate(param0, param1, param2), dimensions={0}
    }
    ENTRY main {
      param0 = f32[200] parameter(0)
      param1 = f32[400] parameter(1)
      param2 = f32[300] parameter(2)
      ROOT fusion = f32[900] fusion(param0, param1, param2),
        calls=fused_computation, kind=kLoop
    }
  )")
                    .value();

  stream_executor::DeviceDescription device_info =
      TestGpuDeviceInfo::RTXA6000DeviceInfo();

  auto* root = module->entry_computation()->root_instruction();
  auto analysis_fused = HloFusionAnalysis::Create(*root, device_info);

  auto emitter =
      GetFusionEmitter(PreBufferAssignmentFusionInfo{analysis_fused});
  auto fusion = dynamic_cast<ConcatenateFusion*>(emitter.get());
  ASSERT_NE(fusion, nullptr);

  constexpr auto kIndexing = R"(
    (th_x, th_y, th_z, bl_x, bl_y, bl_z)[chunk_id, unroll_id] ->
      (bl_x * 128 + th_x),
    domain:
    th_x in [0, 127],
    th_y in [0, 0],
    th_z in [0, 0],
    bl_x in [0, 3],
    bl_y in [0, 0],
    bl_z in [0, 0],
    chunk_id in [0, 0],
    unroll_id in [0, 0],
    bl_x * 128 + th_x in [0, 399]
  )";
  mlir::SmallVector<std::string> dim_names = {"th_x", "th_y", "th_z",
                                              "bl_x", "bl_y", "bl_z"};
  mlir::SmallVector<std::string> range_names = {"chunk_id", "unroll_id"};
  EXPECT_THAT(
      ToString(*fusion->ComputeThreadIdToInputIndexing(
                   /*root_index=*/0, /*hero_operand_index=*/0, &mlir_context_),
               dim_names, range_names, {}),
      MatchIndexingString(kIndexing));
  EXPECT_THAT(
      ToString(*fusion->ComputeThreadIdToInputIndexing(
                   /*root_index=*/0, /*hero_operand_index=*/1, &mlir_context_),
               dim_names, range_names, {}),
      MatchIndexingString(kIndexing));
  EXPECT_THAT(
      ToString(*fusion->ComputeThreadIdToInputIndexing(
                   /*root_index=*/0, /*hero_operand_index=*/2, &mlir_context_),
               dim_names, range_names, {}),
      MatchIndexingString(kIndexing));
}

}  // namespace
}  // namespace gpu
}  // namespace xla
