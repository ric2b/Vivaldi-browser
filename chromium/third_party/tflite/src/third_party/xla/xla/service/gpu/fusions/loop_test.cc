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

#include <memory>
#include <optional>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "absl/status/statusor.h"
#include "mlir/IR/MLIRContext.h"  // from @llvm-project
#include "xla/service/gpu/fusions/fusion_emitter.h"
#include "xla/service/gpu/fusions/fusions.h"
#include "xla/service/gpu/gpu_device_info_for_tests.h"
#include "xla/service/gpu/hlo_fusion_analysis.h"
#include "xla/service/gpu/model/affine_map_printer.h"
#include "xla/service/gpu/model/indexing_test_utils.h"
#include "xla/status_macros.h"
#include "xla/stream_executor/device_description.h"
#include "xla/tests/hlo_test_base.h"
#include "tsl/platform/statusor.h"

namespace xla {
namespace gpu {
namespace {

class LoopTest : public HloTestBase {
 public:
  void SetUp() override {
    HloTestBase::SetUp();

    printer_ =
        AffineMapPrinter({"th_x", "th_y", "th_z", "bl_x", "bl_y", "bl_z"},
                         {"chunk_id", "unroll_id"});
  }

 protected:
  stream_executor::DeviceDescription device_info_ =
      TestGpuDeviceInfo::RTXA6000DeviceInfo();
  AffineMapPrinter printer_;
  mlir::MLIRContext mlir_context_;
};

absl::StatusOr<std::unique_ptr<KernelFusionInterface>> GetFusion(
    const HloFusionAnalysis& analysis) {
  auto emitter = GetFusionEmitter(PreBufferAssignmentFusionInfo{analysis});
  auto fusion = dynamic_cast<KernelFusionInterface*>(emitter.get());
  TF_RET_CHECK(fusion != nullptr);

  emitter.release();
  return std::unique_ptr<KernelFusionInterface>{fusion};
}

TEST_F(LoopTest, ThreadIndexingUnrolled) {
  auto module = ParseAndReturnVerifiedModule(R"(
    HloModule module

    neg {
      %input = f32[100,200,300] parameter(0)
      ROOT neg = f32[100,200,300] negate(%input)
    }

    ENTRY entry {
      %input = f32[100,200,300] parameter(0)
      ROOT %fusion = f32[100,200,300] fusion(%input), kind=kLoop, calls=neg
    })")
                    .value();

  auto* root = module->entry_computation()->root_instruction();
  auto analysis = AnalyzeFusion(*root, device_info_);

  TF_ASSERT_OK_AND_ASSIGN(auto loop_fusion, GetFusion(analysis));
  auto thread_id_to_output_indexing =
      loop_fusion->ComputeThreadIdToOutputIndexing(/*root_index=*/0,
                                                   &mlir_context_);

  EXPECT_THAT(thread_id_to_output_indexing->ToString(printer_),
              MatchIndexingString(R"(
  (th_x, th_y, th_z, bl_x, bl_y, bl_z)[chunk_id, unroll_id] -> (
    (bl_x * 128 + chunk_id * 129024 + th_x) floordiv 15000,
    ((bl_x * 128 + chunk_id * 129024 + th_x) floordiv 75) mod 200,
    ((bl_x * 128 + chunk_id * 129024 + th_x) mod 75) * 4 + unroll_id
  )
  domain:
  th_x in [0, 128)
  th_y in [0, 1)
  th_z in [0, 1)
  bl_x in [0, 1008)
  bl_y in [0, 1)
  bl_z in [0, 1)
  chunk_id in [0, 12)
  unroll_id in [0, 4)
  bl_x * 128 + chunk_id * 129024 + th_x in [0, 1500000)
)"));
}

TEST_F(LoopTest, ThreadIndexingNotUnrolled) {
  auto module = ParseAndReturnVerifiedModule(R"(
    HloModule module

    neg {
      %input = f32[20] parameter(0)
      ROOT neg = f32[20] negate(%input)
    }

    ENTRY entry {
      %input = f32[20] parameter(0)
      ROOT %fusion = f32[20] fusion(%input), kind=kLoop, calls=neg
    })")
                    .value();

  auto* root = module->entry_computation()->root_instruction();
  auto analysis = AnalyzeFusion(*root, device_info_);

  TF_ASSERT_OK_AND_ASSIGN(auto loop_fusion, GetFusion(analysis));
  auto thread_id_to_output_indexing =
      loop_fusion->ComputeThreadIdToOutputIndexing(/*root_index=*/0,
                                                   &mlir_context_);
  EXPECT_THAT(thread_id_to_output_indexing->ToString(printer_),
              MatchIndexingString(R"(
              (th_x, th_y, th_z, bl_x, bl_y, bl_z)[chunk_id, unroll_id] -> (th_x)
              domain:
              th_x in [0, 20)
              th_y in [0, 1)
              th_z in [0, 1)
              bl_x in [0, 1)
              bl_y in [0, 1)
              bl_z in [0, 1)
              chunk_id in [0, 1)
              unroll_id in [0, 1)
            )"));
  auto thread_id_to_input_indexing =
      loop_fusion->ComputeThreadIdToInputIndexing(
          /*root_index=*/0, /*hero_operand_index=*/0, &mlir_context_);
  EXPECT_THAT(thread_id_to_input_indexing->ToString(printer_),
              MatchIndexingString(R"(
              (th_x, th_y, th_z, bl_x, bl_y, bl_z)[chunk_id, unroll_id] -> (th_x)
              domain:
              th_x in [0, 20)
              th_y in [0, 1)
              th_z in [0, 1)
              bl_x in [0, 1)
              bl_y in [0, 1)
              bl_z in [0, 1)
              chunk_id in [0, 1)
              unroll_id in [0, 1)
            )"));
}

TEST_F(LoopTest, Broadcast) {
  auto module = ParseAndReturnVerifiedModule(R"(
    HloModule module

    bcast {
      %input = f32[20] parameter(0)
      ROOT bcast = f32[10, 20, 30] broadcast(%input), dimensions={1}
    }

    ENTRY entry {
      %input = f32[20] parameter(0)
      ROOT %fusion = f32[10, 20, 30] fusion(%input), kind=kLoop, calls=bcast
    })")
                    .value();

  auto* root = module->entry_computation()->root_instruction();
  auto analysis = AnalyzeFusion(*root, device_info_);

  TF_ASSERT_OK_AND_ASSIGN(auto loop_fusion, GetFusion(analysis));
  auto thread_id_to_output_indexing =
      loop_fusion->ComputeThreadIdToOutputIndexing(/*root_index=*/0,
                                                   &mlir_context_);
  EXPECT_THAT(thread_id_to_output_indexing->ToString(printer_),
              MatchIndexingString(R"(
              (th_x, th_y, th_z, bl_x, bl_y, bl_z)[chunk_id, unroll_id] -> (
                (bl_x * 128 + th_x) floordiv 600,
                ((bl_x * 128 + th_x) floordiv 30) mod 20,
                (bl_x * 128 + th_x) mod 30)
                domain:
                th_x in [0, 128)
                th_y in [0, 1)
                th_z in [0, 1)
                bl_x in [0, 47)
                bl_y in [0, 1)
                bl_z in [0, 1)
                chunk_id in [0, 1)
                unroll_id in [0, 1)
                bl_x * 128 + th_x in [0, 6000)
            )"));
  auto thread_id_to_input_indexing =
      loop_fusion->ComputeThreadIdToInputIndexing(
          /*root_index=*/0, /*hero_operand_index=*/0, &mlir_context_);
  EXPECT_THAT(thread_id_to_input_indexing->ToString(printer_),
              MatchIndexingString(R"(
              (th_x, th_y, th_z, bl_x, bl_y, bl_z)[chunk_id, unroll_id] ->
                  (((bl_x * 128 + th_x) floordiv 30) mod 20)
                domain:
                th_x in [0, 128)
                th_y in [0, 1)
                th_z in [0, 1)
                bl_x in [0, 47)
                bl_y in [0, 1)
                bl_z in [0, 1)
                chunk_id in [0, 1)
                unroll_id in [0, 1)
                bl_x * 128 + th_x in [0, 6000)
            )"));
}

}  // namespace
}  // namespace gpu
}  // namespace xla
