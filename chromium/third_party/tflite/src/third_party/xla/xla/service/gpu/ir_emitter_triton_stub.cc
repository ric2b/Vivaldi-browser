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

#include <cstdint>
#include <functional>
#include <optional>
#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Module.h"
#include "mlir/IR/Builders.h"  // from @llvm-project
#include "mlir/IR/BuiltinOps.h"  // from @llvm-project
#include "mlir/IR/ImplicitLocOpBuilder.h"  // from @llvm-project
#include "mlir/IR/MLIRContext.h"  // from @llvm-project
#include "mlir/IR/OwningOpRef.h"  // from @llvm-project
#include "mlir/IR/Value.h"  // from @llvm-project
#include "mlir/IR/ValueRange.h"  // from @llvm-project
#include "mlir/Pass/PassManager.h"  // from @llvm-project
#include "xla/autotuning.pb.h"
#include "xla/hlo/ir/hlo_instructions.h"
#include "xla/service/gpu/hlo_traversal.h"
#include "xla/service/gpu/ir_emitter_triton.h"
#include "xla/service/gpu/launch_dimensions.h"
#include "xla/service/gpu/matmul_utils.h"
#include "xla/service/gpu/model/tiled_hlo_computation.h"
#include "xla/service/gpu/model/tiled_hlo_instruction.h"
#include "xla/service/gpu/triton_fusion_analysis.h"
#include "xla/service/hlo_module_config.h"
#include "xla/stream_executor/device_description.h"
#include "xla/stream_executor/launch_dim.h"
#include "triton/Dialect/Triton/IR/Dialect.h"

namespace xla {
namespace gpu {

absl::Status EmitGeneric(mlir::OpBuilder b, absl::string_view libdevice_path,
                         const se::DeviceDescription& device_info,
                         const HloFusionInstruction* fusion,
                         mlir::triton::FuncOp fn,
                         const BlockLevelParameters& block_level_parameters) {
  return absl::UnimplementedError("not supported for this build configuration");
}

absl::StatusOr<LaunchDimensions> GetMatMulLaunchDimensions(
    const TritonFusionAnalysis& analysis, const HloFusionAdaptor& fusion,
    const TritonGemmConfig& config) {
  return absl::UnimplementedError("not supported for this build configuration");
}

absl::Status EmitMatMul(mlir::OpBuilder b, absl::string_view libdevice_path,
                        const se::DeviceDescription& device_info,
                        const HloFusionInstruction* fusion,
                        mlir::triton::FuncOp fn,
                        const BlockLevelParameters& block_level_parameters) {
  return absl::UnimplementedError("not supported for this build configuration");
}

absl::Status EmitSoftMax(mlir::OpBuilder b, absl::string_view libdevice_path,
                         const se::DeviceDescription& device_info,
                         const HloFusionInstruction* fusion,
                         mlir::triton::FuncOp fn,
                         const BlockLevelParameters& block_level_parameters) {
  return absl::UnimplementedError("not supported for this build configuration");
}

void LoadMlirDialectsForTriton(mlir::MLIRContext& mlir_context) {}

absl::StatusOr<TritonWrapperResult> TritonWrapper(
    absl::string_view fn_name, const HloFusionInstruction* fusion,
    const se::GpuComputeCapability& cc,
    const se::DeviceDescription& device_info,
    const BlockLevelParameters& block_level_parameters,
    llvm::Module* llvm_module, mlir::MLIRContext& mlir_context) {
  return absl::UnimplementedError("not supported for this build configuration");
}

absl::StatusOr<mlir::OwningOpRef<mlir::ModuleOp>> CreateTritonModule(
    absl::string_view fn_name, const HloFusionInstruction* fusion,
    const se::DeviceDescription& device_info,
    const BlockLevelParameters& block_level_parameters,
    mlir::MLIRContext& mlir_context) {
  return absl::UnimplementedError("not supported for this build configuration");
}

absl::StatusOr<TritonWrapperResult> CompileTritonToLLVM(
    const HloModuleConfig& hlo_config, absl::string_view hlo_module_name,
    const se::GpuComputeCapability& cc,
    const se::DeviceDescription& device_info,
    const BlockLevelParameters& block_level_parameters,
    mlir::ModuleOp triton_module, llvm::Module* llvm_module,
    mlir::MLIRContext& mlir_context) {
  return absl::UnimplementedError("not supported for this build configuration");
}

absl::Status CreateTritonPipeline(
    mlir::OpPassManager& pm, const se::GpuComputeCapability& cc,
    const BlockLevelParameters& block_level_parameters,
    mt::nvidia_gpu::ClusterInfo& out_cluster_info) {
  return absl::UnimplementedError("not supported for this build configuration");
}

std::string GetLibdevicePath(const HloModuleConfig& hlo_config,
                             const se::DeviceDescription& device_info) {
  return "";
}

namespace ir_emitter_triton_internal {

MakeTensorPtrOpAndBoundaryChecks CreateMakeTensorPtrOp(
    mlir::ImplicitLocOpBuilder& b, mlir::ValueRange tile_multi_index,
    const TiledHloInstruction& tiled_hlo, mlir::Value argument_block) {
  return MakeTensorPtrOpAndBoundaryChecks();
}
}  // namespace ir_emitter_triton_internal

}  // namespace gpu
}  // namespace xla
