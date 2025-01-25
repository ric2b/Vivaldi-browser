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
#include "xla/service/gpu/fusions/input_slices_mlir.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "xla/error_spec.h"
#include "xla/service/gpu/fusions/mlir_emitter_test_base.h"
#include "xla/service/gpu/hlo_fusion_analysis.h"
#include "xla/service/gpu/model/indexing_test_utils.h"

namespace xla {
namespace gpu {
namespace {

using MlirInputSlicesFusionTest = MlirEmitterTestBase<MlirInputSlicesFusion>;

TEST_F(MlirInputSlicesFusionTest, ThreadIndexing) {
  auto module = ParseAndReturnVerifiedModule(R"(
    HloModule module

    fused_computation {
      %input = f32[4,5] parameter(0)
      slice0 = f32[3,3] slice(input), slice={[1:4],[0:3]}
      slice1 = f32[2,3] slice(input), slice={[0:2],[0:3]}
      ROOT tuple = (f32[3,3], f32[2,3]) tuple(slice0, slice1)
    }

    ENTRY entry {
      %input = f32[4,5] parameter(0)
      ROOT %fusion = (f32[3,3], f32[2,3]) fusion(%input), kind=kLoop, calls=fused_computation
    })")
                    .value();

  auto* root = module->entry_computation()->root_instruction();
  auto analysis = AnalyzeFusion(*root, device_info_);

  auto emitter = GetEmitter(analysis);

  auto thread_id_to_output_indexing_0 =
      emitter->ComputeThreadIdToOutputIndexing(0, &mlir_context_);
  thread_id_to_output_indexing_0->Simplify();
  EXPECT_THAT(thread_id_to_output_indexing_0->ToString(thread_id_printer_),
              MatchIndexingString(R"(
    (th_x, th_y, th_z, bl_x, bl_y, bl_z)[s0, s1] -> (
      th_x floordiv 5 - 1,
      th_x mod 5
    )
    domain:
    th_x in [5, 20)
    th_y in [0, 1)
    th_z in [0, 1)
    bl_x in [0, 1)
    bl_y in [0, 1)
    bl_z in [0, 1)
    s0 in [0, 1)
    s1 in [0, 1)
    th_x mod 5 in [0, 3)
  )"));
  auto thread_id_to_output_indexing_1 =
      emitter->ComputeThreadIdToOutputIndexing(1, &mlir_context_);
  thread_id_to_output_indexing_1->Simplify();
  EXPECT_THAT(thread_id_to_output_indexing_1->ToString(thread_id_printer_),
              MatchIndexingString(R"(
    (th_x, th_y, th_z, bl_x, bl_y, bl_z)[s0, s1] -> (
      th_x floordiv 5,
      th_x mod 5
    )
    domain:
    th_x in [0, 10)
    th_y in [0, 1)
    th_z in [0, 1)
    bl_x in [0, 1)
    bl_y in [0, 1)
    bl_z in [0, 1)
    s0 in [0, 1)
    s1 in [0, 1)
    th_x mod 5 in [0, 3)
  )"));
}

TEST_F(MlirInputSlicesFusionTest, SimpleInputSlices) {
  auto kHloString = R"(
    HloModule module

    fused_computation {
      %input = f32[2,4,5,7]{2,1,0,3} parameter(0)
      slice0 = f32[1,3,3,5]{2,1,0,3} slice(input), slice={[0:1],[1:4],[0:3],[2:7]}
      slice1 = f32[1,2,3,5]{2,1,0,3} slice(input), slice={[0:1],[0:2],[0:3],[2:7]}
      ROOT tuple = (f32[1,3,3,5]{2,1,0,3}, f32[1,2,3,5]{2,1,0,3}) tuple(slice0, slice1)
    }
    ENTRY entry {
      %input = f32[2,4,5,7]{2,1,0,3} parameter(0)
      ROOT %fusion = (f32[1,3,3,5]{2,1,0,3}, f32[1,2,3,5]{2,1,0,3}) fusion(%input), kind=kLoop, calls=fused_computation
    }
  )";
  EXPECT_TRUE(RunAndCompareNoHloPasses(kHloString, ErrorSpec{1e-3}));
}

TEST_F(MlirInputSlicesFusionTest, SliceOfPad) {
  auto kHloString = R"(
    fusion {
      %p0 = f32[6] parameter(0)
      %c0 = f32[] constant(0)
      %pad0 = f32[12] pad(%p0, %c0), padding=0_1_1
      %slice0 = f32[11] slice(%pad0), slice={[1:12]}
      %pad1 = f32[12] pad(%p0, %c0), padding=1_0_1
      %slice1 = f32[11] slice(%pad1), slice={[1:12]}
      ROOT %tuple.9 = (f32[11], f32[11]) tuple(%slice0, %slice1)
    }

    ENTRY entry {
      input = f32[6] parameter(0)
      ROOT fusion = (f32[11], f32[11]) fusion(input), kind=kLoop, calls=fusion
    })";
  EXPECT_TRUE(RunAndCompareNoHloPasses(kHloString, ErrorSpec{1e-3}));
}

TEST_F(MlirInputSlicesFusionTest, ZeroSlice) {
  auto kHloString = R"(
    fusion {
      %p0 = s32[0] parameter(0)
      %p1 = s32[2] parameter(1)
      %concatenate = s32[2] concatenate(p0, p1), dimensions={0}
      %slice = s32[0] slice(%concatenate), slice={[0:0]}
      %slice.1 = s32[2] slice(%concatenate), slice={[0:2]}
      ROOT %tuple = (s32[0], s32[2]) tuple(%slice, %slice.1)
    }

    ENTRY entry {
      %p0 = s32[0] parameter(0)
      %p1 = s32[2] parameter(1)
      ROOT fusion = (s32[0], s32[2]) fusion(%p0, %p1), kind=kLoop, calls=fusion
    })";
  EXPECT_TRUE(RunAndCompareNoHloPasses(kHloString, ErrorSpec{1e-3}));
}

}  // namespace
}  // namespace gpu
}  // namespace xla
