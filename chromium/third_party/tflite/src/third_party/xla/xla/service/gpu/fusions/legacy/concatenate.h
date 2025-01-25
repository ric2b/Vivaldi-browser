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
#ifndef XLA_SERVICE_GPU_FUSIONS_LEGACY_CONCATENATE_H_
#define XLA_SERVICE_GPU_FUSIONS_LEGACY_CONCATENATE_H_

#include <optional>
#include <vector>

#include "absl/status/status.h"
#include "llvm/IR/IRBuilder.h"
#include "mlir/IR/MLIRContext.h"
#include "xla/hlo/ir/hlo_instructions.h"
#include "xla/service/gpu/fusions/fusion_emitter.h"
#include "xla/service/gpu/hlo_fusion_analysis.h"
#include "xla/service/gpu/ir_emitter_context.h"
#include "xla/service/gpu/launch_dimensions.h"
#include "xla/service/gpu/model/indexing_map.h"
#include "xla/service/llvm_ir/ir_array.h"
#include "xla/shape.h"

namespace xla {
namespace gpu {

const Shape& GetLargestConcatOperandShape(const HloFusionAnalysis& analysis);

// Emits a kernel for the given hlo instruction where each thread produces
// one element of each concat operand.
class ConcatenateFusion : public KernelFusionEmitterBase {
 public:
  explicit ConcatenateFusion(const HloFusionAnalysis& analysis);
  LaunchDimensions launch_dimensions() const override;

  std::optional<IndexingMap> ComputeThreadIdToOutputIndexing(
      int64_t root_index, mlir::MLIRContext* ctx) const override;

  std::optional<IndexingMap> ComputeThreadIdToInputIndexing(
      int64_t root_index, int64_t hero_operand_index,
      mlir::MLIRContext* ctx) const override;

 protected:
  absl::Status EmitKernel(IrEmitterContext& ir_emitter_context,
                          const HloFusionInstruction& fusion,
                          const LaunchDimensions& launch_dims,
                          std::vector<llvm_ir::IrArray> inputs,
                          std::vector<llvm_ir::IrArray> outputs,
                          llvm::IRBuilder<>* builder) const override;

 private:
  const HloFusionAnalysis& analysis_;
};

}  // namespace gpu
}  // namespace xla

#endif  // XLA_SERVICE_GPU_FUSIONS_LEGACY_CONCATENATE_H_
