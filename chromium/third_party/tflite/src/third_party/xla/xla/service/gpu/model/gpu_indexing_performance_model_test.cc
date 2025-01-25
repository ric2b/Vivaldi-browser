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

#include "xla/service/gpu/model/gpu_indexing_performance_model.h"

#include <cstdint>
#include <memory>
#include <variant>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "mlir/IR/MLIRContext.h"  // from @llvm-project
#include "xla/hlo/ir/hlo_computation.h"
#include "xla/hlo/ir/hlo_instruction.h"
#include "xla/service/gpu/backend_configs.pb.h"
#include "xla/service/gpu/gpu_device_info_for_tests.h"
#include "xla/service/gpu/hlo_traversal.h"
#include "xla/service/gpu/launch_dimensions.h"
#include "xla/service/gpu/model/fusion_analysis_cache.h"
#include "xla/service/gpu/model/gpu_hlo_cost_analysis.h"
#include "xla/service/gpu/model/gpu_performance_model_base.h"
#include "xla/shape.h"
#include "xla/shape_util.h"
#include "xla/stream_executor/device_description.h"
#include "xla/tests/hlo_test_base.h"
#include "tsl/platform/status_matchers.h"
#include "tsl/platform/statusor.h"

namespace xla {
namespace gpu {
namespace {

using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::tsl::testing::StatusIs;

class GpuIndexingPerformanceModelTest : public HloTestBase {
  GpuHloCostAnalysis::ShapeSizeFunction ShapeSizeBytesFunction() const {
    return [&](const Shape& shape) {
      constexpr int64_t kPointerSize = 8;
      return ShapeUtil::ByteSizeOf(shape, kPointerSize);
    };
  }

 public:
  mlir::MLIRContext mlir_context_;
  // The reference times in the test cases below are measured
  // on A6000 by profiling the execution of the HLOs.
  se::DeviceDescription device_info_{TestGpuDeviceInfo::RTXA6000DeviceInfo()};
  HloFusionAnalysisCache fusion_analysis_cache_{device_info_};
  GpuPerformanceModelWithIndexingAnalysis indexing_cost_model_{
      &device_info_, &fusion_analysis_cache_, ShapeSizeBytesFunction(),
      &mlir_context_};

  GpuIndexingPerformanceModelTest() : HloTestBase() {}
};

TEST_F(GpuIndexingPerformanceModelTest, BroadcastElementwise) {
  TF_ASSERT_OK_AND_ASSIGN(auto module, ParseAndReturnVerifiedModule(
                                           R"(
HloModule extracted

ENTRY entry_computation {
  param_0 = f32[32]{0} parameter(0)
  broadcast = f32[32,1,768]{2,1,0} broadcast(param_0), dimensions={0}
  param_1 = f32[32,1,768]{2,1,0} parameter(1)
  ROOT multiply = f32[32,1,768]{2,1,0} multiply(broadcast, param_1)
}
)"));

  auto producer =
      module->entry_computation()->GetInstructionWithName("broadcast");
  auto consumer =
      module->entry_computation()->GetInstructionWithName("multiply");

  auto runtime_data = indexing_cost_model_.EstimateRunTimeForProducerConsumer(
      producer, consumer);
  EXPECT_EQ(runtime_data.flops, 73728);
  EXPECT_EQ(runtime_data.bytes_written, 98304);
  EXPECT_NEAR(absl::ToInt64Nanoseconds(runtime_data.write_time), 128, 2);
  EXPECT_NEAR(absl::ToInt64Nanoseconds(runtime_data.exec_time), 267, 2);
}

TEST_F(GpuIndexingPerformanceModelTest, Bitcast) {
  TF_ASSERT_OK_AND_ASSIGN(auto module, ParseAndReturnVerifiedModule(
                                           R"(
HloModule m

ENTRY entry_computation {
  param_0 = bf16[4,8,65,128]{3,2,1,0} parameter(0)
  ROOT bitcast = bf16[8,4,65,128]{3,2,0,1} bitcast(param_0)
}
)"));

  auto instruction =
      module->entry_computation()->GetInstructionWithName("bitcast");

  auto runtime_data =
      indexing_cost_model_.EstimateRunTimeForInstruction(instruction);
  EXPECT_EQ(runtime_data.flops, 0);
  EXPECT_EQ(runtime_data.bytes_written, 0);
  EXPECT_EQ(runtime_data.write_time, absl::ZeroDuration());
  EXPECT_EQ(runtime_data.exec_time, absl::ZeroDuration());
}

TEST_F(GpuIndexingPerformanceModelTest, Reduce) {
  TF_ASSERT_OK_AND_ASSIGN(auto module, ParseAndReturnVerifiedModule(
                                           R"(
HloModule m

add {
  param_0 = f32[] parameter(0)
  param_1 = f32[] parameter(1)
  ROOT add.0 = f32[] add(param_0, param_1)
}

ENTRY entry_computation {
  param_0.3 = f32[32,40]{1,0} parameter(0)
  constant = f32[] constant(0)
  ROOT reduce = f32[32]{0} reduce(param_0.3, constant), dimensions={1}, to_apply=add
}
)"));

  auto instruction = module->entry_computation()->root_instruction();

  auto runtime_data =
      indexing_cost_model_.EstimateRunTimeForInstruction(instruction);
  EXPECT_EQ(runtime_data.flops, 3744);
  EXPECT_EQ(runtime_data.bytes_written, 128);
  EXPECT_NEAR(absl::ToDoubleNanoseconds(runtime_data.write_time), 0, 1);
  EXPECT_NEAR(absl::ToDoubleNanoseconds(runtime_data.exec_time), 29, 1);
}

TEST_F(GpuIndexingPerformanceModelTest, VariadicReduce) {
  TF_ASSERT_OK_AND_ASSIGN(auto module, ParseAndReturnVerifiedModule(
                                           R"(
HloModule m

add {
  param_0 = f32[] parameter(0)
  param_1 = f32[] parameter(1)
  param_2 = f32[] parameter(2)
  param_3 = f32[] parameter(3)
  add.0 = f32[] add(param_0, param_2)
  add.1 = f32[] add(param_1, param_3)
  ROOT t = (f32[], f32[]) tuple(add.0, add.1)
}

ENTRY entry_computation {
  param_0.3 = f32[32,40]{1,0} parameter(0)
  param_1.3 = f32[32,40]{1,0} parameter(1)
  param_2.2 = f32[] parameter(2)
  constant = f32[] constant(0)
  ROOT reduce = (f32[32]{0}, f32[32]{0}) reduce(param_0.3, param_1.3, param_2.2, constant), dimensions={1}, to_apply=add
}
)"));

  auto instruction = module->entry_computation()->root_instruction();

  auto runtime_data =
      indexing_cost_model_.EstimateRunTimeForInstruction(instruction);
  EXPECT_EQ(runtime_data.flops, 7488);
  EXPECT_EQ(runtime_data.bytes_written, 256);
  EXPECT_NEAR(absl::ToDoubleNanoseconds(runtime_data.write_time), 0, 1);
  EXPECT_NEAR(absl::ToDoubleNanoseconds(runtime_data.exec_time), 58, 1);
}

TEST_F(GpuIndexingPerformanceModelTest,
       TritonSoftmaxFusionInstructionIsSupported) {
  TF_ASSERT_OK_AND_ASSIGN(auto module, ParseAndReturnVerifiedModule(R"(
HloModule m

add {
  Arg_0 = f32[] parameter(0)
  Arg_1 = f32[] parameter(1)
  ROOT add = f32[] add(Arg_0, Arg_1)
}

triton_softmax_computation {
  param_0 = f32[512,911]{1,0} parameter(0)
  param_1 = f32[911]{0} parameter(1)
  broadcast_0 = f32[512,911]{1,0} broadcast(param_1), dimensions={1}
  multiply_0 = f32[512,911]{1,0} multiply(param_0, broadcast_0)
  constant_0 = f32[] constant(0)
  reduce_0 = f32[512]{0} reduce(multiply_0, constant_0), dimensions={1}, to_apply=add
  broadcast_4 = f32[512,911]{1,0} broadcast(reduce_0), dimensions={0}
  ROOT multiply = f32[512,911]{1,0} multiply(multiply_0, broadcast_4)
}

ENTRY main {
  param_0 = f32[512,911]{1,0} parameter(0)
  param_1 = f32[911]{0} parameter(1)
  ROOT triton_softmax = f32[512,911]{1,0} fusion(param_0, param_1), kind=kCustom, calls=triton_softmax_computation, backend_config={"fusion_backend_config": {"kind":"__triton","block_level_fusion_config":{"output_tile_sizes":["1","911"],"num_warps":"2"}}}
}
)"));
  TF_ASSERT_OK_AND_ASSIGN(auto runtime_data,
                          indexing_cost_model_.EstimateRunTimeForTriton(
                              module->entry_computation()->root_instruction()));

  constexpr int64_t kParam0SizeBytes = 512 * 911 * 4;
  constexpr int64_t kParam1SizeBytes = 911 * 4;
  constexpr int64_t kOutputSizeBytes = 512 * 911 * 4;

  // Each block reads 1 tile of shape [1, 911] from param_0 and full param_1.
  // In total param_0 is read once and param_1 is read 512 times.
  constexpr int64_t kExpectedBytesRead =
      kParam0SizeBytes + 512 * kParam1SizeBytes;

  EXPECT_EQ(runtime_data.bytes_read, kExpectedBytesRead);
  EXPECT_EQ(runtime_data.bytes_written, kOutputSizeBytes);
  EXPECT_NEAR(absl::ToDoubleMicroseconds(runtime_data.exec_time), 5, 1);
}

TEST_F(GpuIndexingPerformanceModelTest,
       TritonSoftmaxProducerConsumerFusionIsSupported) {
  TF_ASSERT_OK_AND_ASSIGN(auto module, ParseAndReturnVerifiedModule(R"(
HloModule m

add {
  Arg_0 = f32[] parameter(0)
  Arg_1 = f32[] parameter(1)
  ROOT add = f32[] add(Arg_0, Arg_1)
}

fusion {
  param_0 = f32[512,911] parameter(0)
  param_1 = f32[911] parameter(1)
  broadcast = f32[512,911] broadcast(param_1), dimensions={1}
  ROOT multiply = f32[512,911] multiply(param_0, broadcast)
}

triton_softmax_computation {
  param_0 = f32[512,911] parameter(0)
  constant_0 = f32[] constant(0)
  reduce_0 = f32[512] reduce(param_0, constant_0), dimensions={1}, to_apply=add
  broadcast_4 = f32[512,911] broadcast(reduce_0), dimensions={0}
  ROOT multiply = f32[512,911] multiply(param_0, broadcast_4)
}

ENTRY main {
  param_0 = f32[512,911] parameter(0)
  param_1 = f32[911] parameter(1)
  fusion.1 = f32[512,911] fusion(param_0, param_1), kind=kLoop, calls=fusion
  ROOT triton_softmax = f32[512,911] fusion(fusion.1), kind=kCustom, calls=triton_softmax_computation, backend_config={"fusion_backend_config": {"kind":"__triton","block_level_fusion_config":{"output_tile_sizes":["1","911"],"num_warps":"2"}}}
}
)"));
  auto consumer = module->entry_computation()->root_instruction();
  auto producer = consumer->operand(0);

  TF_ASSERT_OK_AND_ASSIGN(
      auto runtime_data,
      indexing_cost_model_.EstimateRunTimeForTriton(producer, consumer));

  constexpr int64_t kParam0SizeBytes = 512 * 911 * 4;
  constexpr int64_t kParam1SizeBytes = 911 * 4;
  constexpr int64_t kOutputSizeBytes = 512 * 911 * 4;

  // Each block reads 1 tile of shape [1, 911] from param_0 and full param_1.
  // In total param_0 is read once and param_1 is read 512 times.
  constexpr int64_t kExpectedBytesRead =
      kParam0SizeBytes + 512 * kParam1SizeBytes;

  EXPECT_EQ(runtime_data.bytes_read, kExpectedBytesRead);
  EXPECT_EQ(runtime_data.bytes_written, kOutputSizeBytes);
  EXPECT_NEAR(absl::ToDoubleMicroseconds(runtime_data.exec_time), 5, 1);
}

TEST_F(GpuIndexingPerformanceModelTest,
       EstimateBestTiling_TritonSoftmax_IsSupported) {
  TF_ASSERT_OK_AND_ASSIGN(auto module, ParseAndReturnVerifiedModule(R"(
HloModule m

add {
  Arg_0 = f32[] parameter(0)
  Arg_1 = f32[] parameter(1)
  ROOT add = f32[] add(Arg_0, Arg_1)
}

triton_softmax_computation {
  param_0 = f32[512,911]{1,0} parameter(0)
  param_1 = f32[911]{0} parameter(1)
  broadcast_0 = f32[512,911]{1,0} broadcast(param_1), dimensions={1}
  multiply_0 = f32[512,911]{1,0} multiply(param_0, broadcast_0)
  constant_0 = f32[] constant(0)
  reduce_0 = f32[512]{0} reduce(multiply_0, constant_0), dimensions={1}, to_apply=add
  broadcast_4 = f32[512,911]{1,0} broadcast(reduce_0), dimensions={0}
  ROOT multiply = f32[512,911]{1,0} multiply(multiply_0, broadcast_4)
}

ENTRY main {
  param_0 = f32[512,911]{1,0} parameter(0)
  param_1 = f32[911]{0} parameter(1)
  ROOT triton_softmax = f32[512,911]{1,0} fusion(param_0, param_1), kind=kCustom, calls=triton_softmax_computation, backend_config={"fusion_backend_config": {"kind":"__triton"}}
}
)"));
  auto fusion_adaptor = HloFusionAdaptor::ForInstruction(
      module->entry_computation()->root_instruction());

  TF_ASSERT_OK_AND_ASSIGN(
      auto tiling_result,
      indexing_cost_model_.TryFindBestTilingForFusion(*fusion_adaptor));

  ASSERT_TRUE(std::holds_alternative<TiledRunTimeData>(tiling_result));

  auto tiled_runtime_data = std::get<TiledRunTimeData>(tiling_result);

  constexpr int64_t kParam0SizeBytes = 512 * 911 * 4;
  constexpr int64_t kParam1SizeBytes = 911 * 4;
  constexpr int64_t kOutputSizeBytes = 512 * 911 * 4;

  // Launch grid consists of 128 blocks. Each block reads 1 tile of shape [4,
  // 911] from param_0 and full param_1. In total param_0 is read once and
  // param_1 is read 128 times.
  constexpr int64_t kExpectedBytesRead =
      kParam0SizeBytes + 128 * kParam1SizeBytes;

  EXPECT_THAT(tiled_runtime_data.block_level_parameters.output_tile_sizes,
              ElementsAre(4, 911));
  EXPECT_EQ(tiled_runtime_data.block_level_parameters.num_warps, 4);

  EXPECT_EQ(tiled_runtime_data.runtime_data.bytes_read, kExpectedBytesRead);
  EXPECT_EQ(tiled_runtime_data.runtime_data.bytes_written, kOutputSizeBytes);
  EXPECT_NEAR(
      absl::ToDoubleMicroseconds(tiled_runtime_data.runtime_data.exec_time), 5,
      1);
}

// This test means to catch integer overflow errors when run with ASan build.
// The checks below are just sanity checks for values.
TEST_F(
    GpuIndexingPerformanceModelTest,
    EstimateRunTimeForTiledFusion_NumberOfTilesLargerThanInt32Max_IsSupported) {
  TF_ASSERT_OK_AND_ASSIGN(auto module, ParseAndReturnVerifiedModule(R"(
HloModule softmax

max_computation {
  arg_0 = f16[] parameter(0)
  arg_1 = f16[] parameter(1)
  ROOT maximum = f16[] maximum(arg_0, arg_1)
}

softmax {
  param_0 = f16[65538,32768]{1,0} parameter(0)
  constant_neg_inf = f16[] constant(-inf)
  reduce = f16[65538]{0} reduce(param_0, constant_neg_inf), dimensions={1}, to_apply=max_computation
  broadcast = f16[65538,32768]{1,0} broadcast(reduce), dimensions={0}
  ROOT subtract = f16[65538,32768]{1,0} subtract(param_0, broadcast)
}

ENTRY main {
  param_0 = f16[65538,32768]{1,0} parameter(0)
  ROOT fusion = f16[65538,32768]{1,0} fusion(param_0), kind=kCustom, calls=softmax
}
)"));
  auto fusion_adaptor = HloFusionAdaptor::ForInstruction(
      module->entry_computation()->root_instruction());

  LaunchDimensions launch_dimensions{65538LL * 32768LL, 32};
  TF_ASSERT_OK_AND_ASSIGN(
      auto runtime_data,
      indexing_cost_model_.EstimateRunTimeForTiledFusion(
          *fusion_adaptor, launch_dimensions, /*output_tile_sizes=*/{1, 1}));

  EXPECT_NEAR(absl::ToDoubleSeconds(runtime_data.read_time), 183, 1);
  EXPECT_NEAR(absl::ToDoubleSeconds(runtime_data.compute_time), 39, 1);
  EXPECT_NEAR(absl::ToDoubleSeconds(runtime_data.exec_time), 185, 1);
}

TEST_F(GpuIndexingPerformanceModelTest,
       EstimateRunTimeForTiledFusion_ConcatenateIsNotSupported) {
  TF_ASSERT_OK_AND_ASSIGN(auto module, ParseAndReturnVerifiedModule(R"(
HloModule m

concatenate_fusion {
  param_0 = f32[32, 128] parameter(0)
  param_1 = f32[64, 128] parameter(1)
  ROOT concatenate = f32[96, 128] concatenate(param_0, param_1), dimensions={0}
}

ENTRY main {
  param_0 = f32[32, 128] parameter(0)
  param_1 = f32[64, 128] parameter(1)
  ROOT fusion = f32[96, 128] fusion(param_0, param_1), kind=kCustom, calls=concatenate_fusion
})"));

  auto fusion_adaptor = HloFusionAdaptor::ForInstruction(
      module->entry_computation()->root_instruction());

  LaunchDimensions launch_dimensions{96, 128};

  auto result = indexing_cost_model_.EstimateRunTimeForTiledFusion(
      *fusion_adaptor, launch_dimensions, /*output_tile_sizes=*/{1, 128});

  // Currently SymbolicTileAnalysis fails for concatenate. Once the analysis
  // gets support of concatenate, this test should fail with an error from
  // `EstimateRunTimeForTiledHloComputation` that propagation of the number of
  // blocks is not supported (b/351342921).
  EXPECT_THAT(result, StatusIs(absl::StatusCode::kFailedPrecondition,
                               HasSubstr("SymbolicTileAnalysis failed")));
}

}  // namespace
}  // namespace gpu
}  // namespace xla
