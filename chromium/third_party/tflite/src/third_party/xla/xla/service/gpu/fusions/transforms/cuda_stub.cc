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
#include <string>

#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"
#include "xla/service/gpu/fusions/transforms/passes.h"
#include "xla/stream_executor/device_description.h"

namespace xla {
namespace gpu {

#define GEN_PASS_DEF_CONVERTFLOATNVIDIAPASS
#include "xla/service/gpu/fusions/transforms/passes.h.inc"

class ConvertFloatNvidiaPass
    : public impl::ConvertFloatNvidiaPassBase<ConvertFloatNvidiaPass> {
 public:
  using ConvertFloatNvidiaPassBase::ConvertFloatNvidiaPassBase;

  void runOnOperation() override {}
};

std::unique_ptr<mlir::Pass> CreateConvertFloatNvidiaPass() {
  return std::make_unique<ConvertFloatNvidiaPass>();
}

std::optional<std::unique_ptr<mlir::Pass>> MaybeCreateConvertFloatNvidiaPass(
    const stream_executor::CudaComputeCapability& cc,
    const std::string& cuda_data_dir) {
  return std::nullopt;
}

}  // namespace gpu
}  // namespace xla
