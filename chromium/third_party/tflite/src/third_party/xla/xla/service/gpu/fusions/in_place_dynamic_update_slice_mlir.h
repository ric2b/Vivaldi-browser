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
#ifndef XLA_SERVICE_GPU_FUSIONS_IN_PLACE_DYNAMIC_UPDATE_SLICE_MLIR_H_
#define XLA_SERVICE_GPU_FUSIONS_IN_PLACE_DYNAMIC_UPDATE_SLICE_MLIR_H_

#include <cstdint>
#include <optional>
#include <vector>

#include "absl/status/status.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"  // from @llvm-project
#include "mlir/IR/MLIRContext.h"  // from @llvm-project
#include "xla/hlo/ir/hlo_instruction.h"
#include "xla/hlo/ir/hlo_instructions.h"
#include "xla/service/gpu/fusions/mlir/computation_partitioner.h"
#include "xla/service/gpu/fusions/mlir/mlir_fusion_emitter.h"
#include "xla/service/gpu/hlo_fusion_analysis.h"
#include "xla/service/gpu/ir_emission_utils.h"
#include "xla/service/gpu/launch_dimensions.h"
#include "xla/service/gpu/model/indexing_map.h"

namespace xla {
namespace gpu {

// Fusion node where the root is either:
// 1. a dynamic-update-slice op
// 2. a bitcast of a dynamic-update-slice op
// 3. a tuple op returning the result of several dynamic-update-slice ops
// 4. a tuple op returning the result of several bitcast
//    dynamic-update-slice ops
//
// Lowers to LLVM via MLIR.
class MlirInPlaceDynamicUpdateSliceFusion : public MlirFusionEmitterBase {
 public:
  explicit MlirInPlaceDynamicUpdateSliceFusion(
      const HloFusionAnalysis& analysis)
      : analysis_(analysis),
        dus_ops_(
            GetOutputDefiningDynamicUpdateSlices(analysis.fusion_roots())) {}

  LaunchDimensions launch_dimensions() const override;

  std::optional<IndexingMap> ComputeThreadIdToOutputIndexing(
      int64_t root_index, mlir::MLIRContext* indexing_context) const override {
    // The mapping cannot be statically computed in general, since the offsets
    // are unknown.
    return std::nullopt;
  }

  std::optional<IndexingMap> ComputeThreadIdToInputIndexing(
      int64_t root_index, int64_t hero_operand_index,
      mlir::MLIRContext* indexing_context) const override;

 protected:
  absl::Status EmitEntryFunction(
      const mlir_converter::PartitionedComputations& computations,
      const mlir_converter::CallTargetProvider& call_targets,
      mlir::func::FuncOp entry_function,
      const HloFusionInstruction& fusion) const override;

  std::vector<mlir_converter::EpilogueSpecification> GetEpilogues(
      const HloFusionInstruction& fusion,
      mlir::MLIRContext* mlir_context) const override;

 private:
  const HloFusionAnalysis& analysis_;
  std::vector<const HloInstruction*> dus_ops_;
};

}  // namespace gpu
}  // namespace xla

#endif  // XLA_SERVICE_GPU_FUSIONS_IN_PLACE_DYNAMIC_UPDATE_SLICE_MLIR_H_
