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

#include "xla/service/spmd/shardy/round_trip_common/convert_sharding_custom_calls.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "llvm/ADT/StringRef.h"
#include "mlir/IR/BuiltinAttributes.h"  // from @llvm-project
#include "mlir/IR/BuiltinOps.h"  // from @llvm-project
#include "mlir/IR/MLIRContext.h"  // from @llvm-project
#include "mlir/IR/OperationSupport.h"  // from @llvm-project
#include "mlir/Pass/Pass.h"  // from @llvm-project
#include "mlir/Pass/PassRegistry.h"  // from @llvm-project
#include "mlir/Support/LLVM.h"  // from @llvm-project
#include "mlir/Support/LogicalResult.h"  // from @llvm-project
#include "mlir/Support/TypeID.h"  // from @llvm-project
#include "mlir/Transforms/DialectConversion.h"  // from @llvm-project
#include "shardy/dialect/sdy/ir/constants.h"  // from @shardy
#include "shardy/dialect/sdy/ir/dialect.h"  // from @shardy
#include "xla/mlir_hlo/mhlo/IR/hlo_ops.h"
#include "xla/service/spmd/shardy/constants.h"
#include "xla/sharding_op_util.h"

namespace xla {
namespace sdy {

namespace {

using ::mlir::StringRef;

using ::mlir::mhlo::CustomCallOp;

using ::mlir::sdy::kShardingAttr;
using ::mlir::sdy::ShardingConstraintOp;
using ::mlir::sdy::TensorShardingAttr;
using ::mlir::sdy::TensorShardingPerValueAttr;

class ShardingCustomCallPattern
    : public mlir::OpConversionPattern<CustomCallOp> {
  using OpConversionPattern::OpConversionPattern;

  mlir::LogicalResult matchAndRewrite(
      CustomCallOp op, OpAdaptor adaptor,
      mlir::ConversionPatternRewriter& rewriter) const override {
    if (op.getCallTargetName() != kShardingCustomCallTargetName) {
      return rewriter.notifyMatchFailure(
          op, "expected CustomCallOp with target name " +
                  kShardingCustomCallTargetName.str());
    }

    CHECK_EQ(op.getNumOperands(), 1);

    std::vector<int64_t> unspecDims;
    if (std::optional<mlir::Attribute> backendConfig = op.getBackendConfig()) {
      CHECK_OK(xla::sharding_op_util::ParseAttributes(
          mlir::dyn_cast<mlir::StringAttr>(*backendConfig).getValue(),
          &unspecDims));
    }

    auto shardingPerValue =
        op->getAttrOfType<TensorShardingPerValueAttr>(kShardingAttr);
    if (!shardingPerValue) {
      op.emitError() << "expected CustomCallOp with sharding attribute";
      return mlir::failure();
    }
    if (shardingPerValue.size() != 1) {
      op.emitError() << "expected CustomCallOp with exactly one sharding "
                        "attribute";
      return mlir::failure();
    }
    TensorShardingAttr sharding = shardingPerValue.getShardings().front();

    if (!unspecDims.empty()) {
      sharding = sharding.openShardingDims(unspecDims);
    }

    rewriter.replaceOpWithNewOp<ShardingConstraintOp>(
        op, adaptor.getInputs().front(), sharding);

    return mlir::success();
  }
};

// Converts a CustomCall with target name Sharding into a
// ShardingConstraintOp.
class ConvertShardingCustomCallsPass
    : public mlir::PassWrapper<ConvertShardingCustomCallsPass,
                               mlir::OperationPass<mlir::ModuleOp>> {
 public:
  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(ConvertShardingCustomCallsPass)

  void runOnOperation() final {
    mlir::MLIRContext& context = getContext();
    mlir::ConversionTarget target(context);
    target.addLegalDialect<mlir::sdy::SdyDialect>();
    target.addDynamicallyLegalOp<CustomCallOp>([](CustomCallOp op) {
      return op.getCallTargetName() != kShardingCustomCallTargetName;
    });
    mlir::RewritePatternSet patterns(&context);
    patterns.add<ShardingCustomCallPattern>(&context);
    if (mlir::failed(mlir::applyPartialConversion(getOperation(), target,
                                                  std::move(patterns)))) {
      signalPassFailure();
    }
  }

  StringRef getArgument() const override {
    return "xla-sdy-convert-sharding-custom-calls";
  }

  StringRef getDescription() const override {
    return "Converts a CustomCall with target name Sharding into a "
           "ShardingConstraintOp.";
  }
};

}  // namespace

std::unique_ptr<mlir::Pass> createConvertShardingCustomCallsPass() {
  return std::make_unique<ConvertShardingCustomCallsPass>();
}

void registerConvertShardingCustomCallsPass() {
  mlir::registerPass(createConvertShardingCustomCallsPass);
}

}  // namespace sdy
}  // namespace xla
