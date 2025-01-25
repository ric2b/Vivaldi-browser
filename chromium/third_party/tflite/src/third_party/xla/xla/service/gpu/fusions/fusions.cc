/* Copyright 2023 The OpenXLA Authors.

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
#include "xla/service/gpu/fusions/fusions.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <utility>

#include "absl/algorithm/container.h"
#include "absl/strings/match.h"
#include "xla/hlo/ir/hlo_instruction.h"
#include "xla/hlo/ir/hlo_opcode.h"
#include "xla/layout_util.h"
#include "xla/service/gpu/backend_configs.pb.h"
#include "xla/service/gpu/fusions/concatenate_mlir.h"
#include "xla/service/gpu/fusions/copy.h"
#include "xla/service/gpu/fusions/cudnn.h"
#include "xla/service/gpu/fusions/custom.h"
#include "xla/service/gpu/fusions/fusion_emitter.h"
#include "xla/service/gpu/fusions/in_place_dynamic_update_slice_mlir.h"
#include "xla/service/gpu/fusions/input_slices_mlir.h"
#include "xla/service/gpu/fusions/legacy/concatenate.h"
#include "xla/service/gpu/fusions/legacy/in_place_dynamic_update_slice.h"
#include "xla/service/gpu/fusions/legacy/input_slices.h"
#include "xla/service/gpu/fusions/legacy/loop.h"
#include "xla/service/gpu/fusions/legacy/reduction.h"
#include "xla/service/gpu/fusions/legacy/scatter.h"
#include "xla/service/gpu/fusions/legacy/transpose.h"
#include "xla/service/gpu/fusions/loop_mlir.h"
#include "xla/service/gpu/fusions/reduction_mlir.h"
#include "xla/service/gpu/fusions/scatter_mlir.h"
#include "xla/service/gpu/fusions/transpose_mlir.h"
#include "xla/service/gpu/fusions/triton.h"
#include "xla/service/gpu/hlo_fusion_analysis.h"
#include "xla/service/gpu/hlo_traversal.h"
#include "xla/service/gpu/ir_emission_utils.h"
#include "xla/shape.h"
#include "xla/shape_util.h"

namespace xla {
namespace gpu {
namespace {

bool IsParameterOrGteOfParameter(const HloInstruction* instr) {
  if (instr->opcode() == HloOpcode::kParameter) {
    return true;
  }
  if (instr->opcode() == HloOpcode::kGetTupleElement) {
    return IsParameterOrGteOfParameter(instr->operand(0));
  }
  return false;
}

bool IsDynamicUpdateSliceFusion(const HloFusionAnalysis& analysis) {
  return absl::c_all_of(
      analysis.fusion_roots(), [](const HloInstructionAdaptor& root) {
        return root.opcode() == HloOpcode::kDynamicUpdateSlice ||
               (root.opcode() == HloOpcode::kBitcast &&
                root.GetOperand(0).opcode() == HloOpcode::kDynamicUpdateSlice);
      });
}

}  // namespace

std::optional<std::unique_ptr<FusionInterface>> HloFusionInfo::GetCopyFusion()
    const {
  for (const HloInstructionAdaptor& root_adaptor : analysis().fusion_roots()) {
    const HloInstruction* root = &root_adaptor.instruction();
    if (root->opcode() != HloOpcode::kCopy ||
        root->operand(0)->opcode() != HloOpcode::kParameter ||
        !LayoutUtil::Equal(root->operand(0)->shape().layout(),
                           root->shape().layout())) {
      return std::nullopt;
    }
  }

  return std::make_unique<MemcpyFusion>(analysis(), buffer_assignment_);
}

bool HloFusionInfo::CanEmitDynamicUpdateSliceInPlace() const {
  auto ret = CanEmitFusedDynamicUpdateSliceInPlaceForGpu(
      analysis().fusion(),
      [this](const HloInstruction* instruction, const ShapeIndex& index) {
        return GetAllocationSlice(*buffer_assignment_, instruction, index);
      },
      instr_);
  return ret.ok() && *ret;
}

std::unique_ptr<FusionInterface> GetFusionEmitter(
    const FusionInfo& fusion_info) {
  const auto& analysis = fusion_info.analysis();
  const FusionBackendConfig& backend_config = analysis.fusion_backend_config();

  const auto& opts = analysis.fusion_root(0)
                         .instruction()
                         .GetModule()
                         ->config()
                         .debug_options();
  auto check_mlir_emitters = [&](int64_t required_level) {
    return opts.xla_gpu_mlir_emitter_level() >= required_level;
  };

  switch (analysis.GetEmitterFusionKind()) {
    case HloFusionAnalysis::EmitterFusionKind::kCustomFusion: {
      const auto& config = backend_config.custom_fusion_config();
      if (absl::StrContains(config.name(), "address_computation")) {
        return std::make_unique<DynamicSliceFusion>(analysis);
      }
      return std::make_unique<CustomFusion>();
    }
    case HloFusionAnalysis::EmitterFusionKind::kInputSlices:
      if (check_mlir_emitters(/*required_level=*/2)) {
        return std::make_unique<MlirInputSlicesFusion>(analysis);
      }
      return std::make_unique<InputSlicesFusion>(analysis);
    case HloFusionAnalysis::EmitterFusionKind::kLoop: {
      if (IsDynamicUpdateSliceFusion(analysis) &&
          fusion_info.CanEmitDynamicUpdateSliceInPlace()) {
        if (check_mlir_emitters(/*required_level=*/2)) {
          return std::make_unique<MlirInPlaceDynamicUpdateSliceFusion>(
              analysis);
        }
        return std::make_unique<InPlaceDynamicUpdateSliceFusion>(analysis);
      }

      if (auto copy_fusion = fusion_info.GetCopyFusion()) {
        return *std::move(copy_fusion);
      }

      if (check_mlir_emitters(/*required_level=*/1)) {
        return std::make_unique<MlirLoopFusion>(analysis);
      }
      return std::make_unique<LoopFusion>(analysis);
    }
    case HloFusionAnalysis::EmitterFusionKind::kReduction:
      if (check_mlir_emitters(/*required_level=*/4)) {
        return CreateMlirReductionFusion(analysis);
      }
      return std::make_unique<ReductionFusion>(analysis);
    case HloFusionAnalysis::EmitterFusionKind::kScatter: {
      if (check_mlir_emitters(/*required_level=*/2)) {
        return std::make_unique<MlirScatterFusion>(analysis);
      }
      return std::make_unique<ScatterFusion>(analysis);
    }
    case HloFusionAnalysis::EmitterFusionKind::kTranspose: {
      if (check_mlir_emitters(/*required_level=*/3)) {
        return std::make_unique<MlirTransposeFusion>(analysis);
      }
      return std::make_unique<TransposeFusion>(analysis.device_info(),
                                               analysis);
    }
    case HloFusionAnalysis::EmitterFusionKind::kConcatenate: {
      if (check_mlir_emitters(/*required_level=*/2)) {
        return std::make_unique<MlirConcatenateFusion>(analysis);
      }
      return std::make_unique<ConcatenateFusion>(analysis);
    }
    case HloFusionAnalysis::EmitterFusionKind::kTriton:
      return std::make_unique<TritonFusion>(analysis);
    case HloFusionAnalysis::EmitterFusionKind::kCuDnn:
      return std::make_unique<CuDnnFusion>(analysis);
  }
}

}  // namespace gpu
}  // namespace xla
