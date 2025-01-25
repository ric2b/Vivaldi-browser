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
#ifndef XLA_SERVICE_GPU_FUSIONS_TRANSPOSE_MLIR_H_
#define XLA_SERVICE_GPU_FUSIONS_TRANSPOSE_MLIR_H_

#include <cstdint>
#include <optional>
#include <vector>

#include "absl/status/status.h"
#include "llvm/ADT/SmallVector.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"  // from @llvm-project
#include "mlir/IR/AffineMap.h"  // from @llvm-project
#include "mlir/IR/ImplicitLocOpBuilder.h"  // from @llvm-project
#include "mlir/IR/MLIRContext.h"  // from @llvm-project
#include "mlir/IR/Value.h"  // from @llvm-project
#include "mlir/IR/ValueRange.h"  // from @llvm-project
#include "mlir/Support/LLVM.h"  // from @llvm-project
#include "xla/hlo/ir/hlo_instructions.h"
#include "xla/service/gpu/fusions/mlir/computation_partitioner.h"
#include "xla/service/gpu/fusions/mlir/mlir_fusion_emitter.h"
#include "xla/service/gpu/hlo_fusion_analysis.h"
#include "xla/service/gpu/ir_emission_utils.h"
#include "xla/service/gpu/launch_dimensions.h"
#include "xla/service/gpu/model/indexing_map.h"
#include "xla/util.h"

namespace xla {
namespace gpu {

// Lowers kTranspose fusion to LLVM via MLIR using GPU's shared memory.

// Each thread block of `kWarpSize` x `kNumRows` threads
// transposes one tile: each thread copies kWarpSize/kNumRows elements from
// the input to a shared memory tile.

// This is similar to the following CUDA algorithm in TensorFlow:
// https://goo.gl/MStRV6.
class MlirTransposeFusion : public MlirFusionEmitterBase {
 public:
  explicit MlirTransposeFusion(const HloFusionAnalysis& analysis);
  LaunchDimensions launch_dimensions() const override;

  std::optional<IndexingMap> ComputeThreadIdToOutputIndexing(
      int64_t root_index, mlir::MLIRContext* mlir_context) const override;

  std::optional<IndexingMap> ComputeThreadIdToInputIndexing(
      int64_t root_index, int64_t hero_operand_index,
      mlir::MLIRContext* mlir_context) const override;

 protected:
  absl::Status EmitEntryFunction(
      const mlir_converter::PartitionedComputations& computations,
      const mlir_converter::CallTargetProvider& call_targets,
      mlir::func::FuncOp entry_function,
      const HloFusionInstruction& fusion) const override;

  std::vector<mlir_converter::EpilogueSpecification> GetEpilogues(
      const HloFusionInstruction& fusion,
      mlir::MLIRContext* mlir_context) const override;

  struct WriteResult {
    // All output tensors of the fusion, with side outputs written to them.
    mlir::SmallVector<mlir::Value> updated_outputs;
    // Shared memory tiles for transpose heroes.
    mlir::ValueRange shmem_tensors;
  };

  WriteResult EmitWriteToShMemMlir(
      mlir::ImplicitLocOpBuilder& builder, mlir::func::FuncOp entry_function,
      const HloFusionInstruction& fusion,
      const mlir_converter::PartitionedComputation& root_computation,
      const mlir_converter::CallTargetProvider& call_target_provider,
      mlir::ValueRange output_args) const;
  void EmitReadFromShMemMlir(
      mlir::ImplicitLocOpBuilder& builder, mlir::func::FuncOp entry_function,
      const HloFusionInstruction& fusion,
      const mlir_converter::PartitionedComputations& computations,
      const WriteResult& written) const;

 private:
  const HloFusionAnalysis& analysis_;

  IndexingMap GetIndexing(bool input, const xla::Shape& shape,
                          mlir::MLIRContext* ctx) const;
  IndexingMap GetSharedMemoryIndexing(bool read, mlir::MLIRContext* ctx) const;
  llvm::SmallVector<mlir::AffineExpr, 4> GetThreadOffsets(
      mlir::MLIRContext* ctx) const;

  TransposeDescription transpose_;
  Vector3 permutation_;
  std::vector<int64_t> input_shape_;
  std::vector<int64_t> block_sizes_;  // In input elements.
  std::vector<int64_t> block_counts_;
  int vector_size_;
  int block_size_;

  std::vector<const HloInstruction*> shmem_transposes_;
  std::vector<const HloInstruction*> shmem_transpose_roots_;
  std::vector<int> shmem_transpose_root_indices_;
  std::vector<const HloInstruction*> side_output_roots_;
  std::vector<int> side_output_root_indices_;
};

}  // namespace gpu
}  // namespace xla

#endif  // XLA_SERVICE_GPU_FUSIONS_TRANSPOSE_MLIR_H_
