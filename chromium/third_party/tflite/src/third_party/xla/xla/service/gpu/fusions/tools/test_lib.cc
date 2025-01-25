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
#include "xla/service/gpu/fusions/tools/test_lib.h"

#include <memory>

#include "absl/algorithm/container.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Complex/IR/Complex.h"
#include "mlir/Dialect/DLTI/DLTI.h"
#include "mlir/Dialect/Func/Extensions/AllExtensions.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/GPU/IR/GPUDialect.h"
#include "mlir/Dialect/Math/IR/Math.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/Dialect/Tensor/IR/Tensor.h"
#include "mlir/Dialect/Vector/IR/VectorOps.h"
#include "mlir/Tools/mlir-opt/MlirOptMain.h"
#include "xla/hlo/ir/hlo_casting_utils.h"
#include "xla/hlo/ir/hlo_computation.h"
#include "xla/hlo/ir/hlo_instruction.h"
#include "xla/hlo/ir/hlo_instructions.h"
#include "xla/hlo/ir/hlo_opcode.h"
#include "xla/mlir_hlo/mhlo/IR/hlo_ops.h"
#include "xla/service/gpu/fusions/fusions.h"
#include "xla/service/gpu/fusions/ir/xla_gpu_ops.h"
#include "xla/service/gpu/gpu_device_info_for_tests.h"
#include "xla/service/gpu/hlo_fusion_analysis.h"
#include "xla/status_macros.h"
#include "xla/tools/hlo_module_loader.h"

namespace xla {
namespace gpu {

absl::StatusOr<std::unique_ptr<HloModule>> LoadTestModule(
    absl::string_view filename) {
  auto module = *xla::LoadModuleFromFile(std::string(filename));
  module->mutable_config()
      .mutable_debug_options()
      .set_xla_gpu_mlir_emitter_level(4);

  int num_fusions = absl::c_count_if(
      module->entry_computation()->instructions(),
      [](const HloInstruction* instruction) {
        return instruction->opcode() == xla::HloOpcode::kFusion;
      });
  TF_RET_CHECK(num_fusions <= 1) << "HLO must contain at most one fusion";

  if (num_fusions == 0) {
    // Generate a fusion from the entry computation.
    HloComputation::Builder builder("generated_main");
    std::vector<HloInstruction*> params;
    for (const auto* param :
         module->entry_computation()->parameter_instructions()) {
      params.push_back(*builder.AddParameter(param->Clone(/*suffix=*/"")));
    }
    builder.AddInstruction(HloInstruction::CreateFusion(
        module->entry_computation()->root_instruction()->shape(),
        HloInstruction::FusionKind::kLoop /* irrelevant */, params,
        module->entry_computation()));

    auto* new_entry = module->AddComputationAndUnifyNamesAndIds(
        builder.Build(), /*is_entry=*/false);
    module->ReplaceEntryComputation(new_entry);
  }

  return module;
}

absl::StatusOr<std::unique_ptr<EmitterData>> GetMlirFusionEmitter(
    const HloModule& module) {
  auto data = std::make_unique<EmitterData>();
  data->fusion = DynCast<HloFusionInstruction>(
      module.entry_computation()->root_instruction());
  TF_RET_CHECK(data->fusion != nullptr) << "Root instruction must be a fusion";
  data->device.emplace(TestGpuDeviceInfo::RTXA6000DeviceInfo());
  data->analysis.emplace(
      HloFusionAnalysis::Create(*data->fusion, data->device.value()));
  PreBufferAssignmentFusionInfo info(data->analysis.value());
  auto emitter = GetFusionEmitter(info);

  auto mlir_emitter = dynamic_cast<MlirFusionEmitterBase*>(emitter.get());
  TF_RET_CHECK(mlir_emitter != nullptr)
      << "Expected emitter to be an MlirFusionEmitter";

  emitter.release();
  data->emitter.reset(mlir_emitter);
  return data;
}

mlir::MLIRContext GetMlirContextForTest() {
  mlir::DialectRegistry registry;
  registry.insert<mlir::DLTIDialect, mlir::tensor::TensorDialect,
                  mlir::func::FuncDialect, mlir::affine::AffineDialect,
                  mlir::arith::ArithDialect, mlir::complex::ComplexDialect,
                  mlir::math::MathDialect, mlir::scf::SCFDialect,
                  mlir::mhlo::MhloDialect, mlir::gpu::GPUDialect,
                  mlir::vector::VectorDialect, XlaGpuDialect>();
  return mlir::MLIRContext(registry);
}

}  // namespace gpu
}  // namespace xla
