/* Copyright 2024 The TensorFlow Authors. All Rights Reserved.

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

#include "xla/service/gpu/fusions/ir/xla_gpu_ops.h"

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/STLFunctionalExtras.h"
#include "llvm/ADT/SmallBitVector.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/LogicalResult.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/IR/AffineExpr.h"
#include "mlir/IR/Builders.h"  // IWYU pragma: keep
#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/DialectImplementation.h"  // IWYU pragma: keep
#include "mlir/IR/MLIRContext.h"  // IWYU pragma: keep
#include "mlir/IR/OpDefinition.h"
#include "mlir/IR/OpImplementation.h"
#include "mlir/IR/OperationSupport.h"
#include "mlir/IR/PatternMatch.h"  // IWYU pragma: keep
#include "mlir/IR/SymbolTable.h"
#include "mlir/IR/TypeRange.h"
#include "mlir/IR/TypeUtilities.h"  // IWYU pragma: keep
#include "mlir/IR/Types.h"
#include "mlir/IR/Value.h"
#include "mlir/IR/ValueRange.h"
#include "mlir/Support/LLVM.h"
#include "mlir/Support/LogicalResult.h"
#include "xla/service/gpu/fusions/ir/xla_gpu_dialect.cc.inc"
#include "xla/service/gpu/model/indexing_map.h"
#include "xla/service/gpu/model/indexing_map_serialization.h"

namespace xla {
namespace gpu {
namespace {

using llvm::ArrayRef;
using mlir::AffineExpr;
using mlir::AffineMap;
using mlir::Block;
using mlir::DenseI64ArrayAttr;
using mlir::failure;
using mlir::getAffineConstantExpr;
using mlir::getAffineDimExpr;
using mlir::getAffineSymbolExpr;
using mlir::Location;
using mlir::LogicalResult;
using mlir::MLIRContext;
using mlir::OpAsmParser;
using mlir::OpAsmPrinter;
using mlir::OpBuilder;
using mlir::OperationState;
using mlir::ParseResult;
using mlir::PatternRewriter;
using mlir::RankedTensorType;
using mlir::Region;
using mlir::SmallVector;
using mlir::success;
using mlir::Type;
using mlir::TypeRange;
using mlir::Value;
using mlir::ValueRange;

namespace arith = mlir::arith;

}  // namespace

LogicalResult PureCallOp::verifySymbolUses(
    mlir::SymbolTableCollection& symbolTable) {
  auto callee = getCalleeAttr();
  auto function =
      symbolTable.lookupNearestSymbolFrom<mlir::func::FuncOp>(*this, callee);
  if (!function) {
    return emitError("'f' attribute refers to an undefined function: ")
           << callee;
  }

  int func_arg_count = function.getFunctionType().getNumInputs();
  int arg_count = getOperands().size();

  if (arg_count != func_arg_count) {
    return emitError() << "argument count mismatch: 'operands' has "
                       << arg_count << " arguments, but '" << callee
                       << "' expects " << func_arg_count;
  }

  return success();
}

//===----------------------------------------------------------------------===//
// AllocateSharedOp
//===----------------------------------------------------------------------===//

void AllocateSharedOp::getAsmResultNames(
    llvm::function_ref<void(mlir::Value, mlir::StringRef)> setNameFn) {
  setNameFn(getResult(), "shmem");
}

//===----------------------------------------------------------------------===//
// ApplyIndexingOp
//===----------------------------------------------------------------------===//

void ApplyIndexingOp::build(OpBuilder& builder, OperationState& result,
                            ValueRange dims, ValueRange symbols,
                            const IndexingMap& indexing_map) {
  SmallVector<Value, 4> operands;
  operands.reserve(dims.size() + symbols.size());
  operands.append(dims.begin(), dims.end());
  operands.append(symbols.begin(), symbols.end());
  build(builder, result, operands, indexing_map);
}

void ApplyIndexingOp::build(OpBuilder& builder, OperationState& result,
                            ValueRange operands,
                            const IndexingMap& indexing_map) {
  SmallVector<Type, 2> result_types(indexing_map.GetAffineMap().getNumResults(),
                                    builder.getIndexType());
  IndexingMapAttr indexing_map_attr =
      IndexingMapAttr::get(builder.getContext(), indexing_map);
  build(builder, result, result_types, operands, indexing_map_attr);
}

void ApplyIndexingOp::build(OpBuilder& builder, OperationState& result,
                            ValueRange operands, AffineMap affine_map,
                            ArrayRef<IndexingMap::Variable> dim_vars,
                            ArrayRef<IndexingMap::Variable> range_vars) {
  IndexingMap indexing_map(affine_map, dim_vars, range_vars, {});
  build(builder, result, operands, indexing_map);
}

// Parses a comma-separated list of operands, ex: %d1, %d2.
ParseResult parseOperands(
    OpAsmParser& parser,
    SmallVector<OpAsmParser::UnresolvedOperand, 4>* operands) {
  OpAsmParser::UnresolvedOperand operand;
  return parser.parseCommaSeparatedList(
      [&]() { return parser.parseOperand(operands->emplace_back()); });
}

ParseResult ApplyIndexingOp::parse(OpAsmParser& parser,
                                   OperationState& result) {
  mlir::Builder& builder = parser.getBuilder();
  auto index_type = builder.getIndexType();

  IndexingMapAttr indexing_map_attr;
  if (parser.parseAttribute(indexing_map_attr, "indexing_map_attr",
                            result.attributes)) {
    return failure();
  }

  SmallVector<OpAsmParser::UnresolvedOperand, 4> operands;
  SmallVector<int64_t, 4> lower_bounds, upper_bounds;
  if (succeeded(parser.parseOptionalLParen())) {
    if (parseOperands(parser, &operands) || parser.parseRParen()) {
      return failure();
    }
  }
  if (succeeded(parser.parseOptionalLSquare())) {
    if (parseOperands(parser, &operands) || parser.parseRSquare()) {
      return failure();
    }
  }
  if (parser.resolveOperands(operands, index_type, result.operands) ||
      parser.parseOptionalAttrDict(result.attributes)) {
    return failure();
  }
  auto map = indexing_map_attr.getIndexingMap().GetAffineMap();
  result.addTypes(SmallVector<Type, 2>(map.getNumResults(), index_type));
  return success();
}

void ApplyIndexingOp::print(OpAsmPrinter& p) {
  AffineMap affine_map = getIndexingMapAttr().getIndexingMap().GetAffineMap();
  p << " " << getIndexingMapAttr();

  auto operands = getOperands();
  unsigned num_dimensions = affine_map.getNumDims();
  if (num_dimensions > 0) {
    p << '(';
    auto dimension_operands = operands.slice(0, num_dimensions);
    llvm::interleaveComma(dimension_operands, p);
    p << ')';
  }

  unsigned num_symbols = affine_map.getNumSymbols();
  if (num_symbols > 0) {
    p << '[';
    auto symbol_operands = operands.slice(num_dimensions, num_symbols);
    llvm::interleaveComma(symbol_operands, p);
    p << ']';
  }

  p.printOptionalAttrDict((*this)->getAttrs(),
                          /*elidedAttrs=*/{"indexing_map_attr"});
}

LogicalResult ApplyIndexingOp::verify() {
  auto affine_map = getIndexingMapAttr().getIndexingMap().GetAffineMap();
  unsigned num_variables = affine_map.getNumDims() + affine_map.getNumSymbols();
  if (getOperands().size() != num_variables) {
    return emitOpError(
        "operand count must match the number of dimensions and symbols in the "
        "affine map");
  }
  if (!getIndexingMap().GetConstraints().empty()) {
    return emitOpError("apply indexing op cannot have any constraints");
  }
  return success();
}

IndexingMap ApplyIndexingOp::getIndexingMap() {
  return getIndexingMapAttr().getIndexingMap();
}

namespace {
struct IndexingMapWithAdditions {
  IndexingMap indexing_map;
  SmallVector<Value> added_dim_args;
  SmallVector<Value> added_sym_args;
};

IndexingMapWithAdditions GetNewIndexingMapAfterFoldingSequence(
    IndexingMap indexing_map,
    SmallVector<std::pair<int, ApplyIndexingOp>, 2> apply_indexing_ops,
    mlir::DenseMap<Value, AffineExpr> operand_exprs, MLIRContext* ctx) {
  int num_dims = indexing_map.GetDimensionCount();
  int num_syms = indexing_map.GetSymbolCount();

  SmallVector<Value> added_dim_args;
  SmallVector<Value> added_sym_args;
  auto new_dim_vars = indexing_map.GetDimVars();
  auto new_sym_vars = indexing_map.GetRangeVars();

  mlir::DenseMap<AffineExpr, AffineExpr> replacements;
  for (auto& [operand_id, producer] : apply_indexing_ops) {
    auto producer_map = producer.getIndexingMap();
    mlir::OpResult producer_result = producer->getOpResult(0);
    int producer_result_id = producer_result.getResultNumber();
    int num_producer_dims = producer.getAffineMap().getNumDims();
    SmallVector<AffineExpr> producer_dim_replacements;
    SmallVector<AffineExpr> producer_sym_replacements;
    for (auto& producer_operand : producer->getOpOperands()) {
      int producer_operand_number = producer_operand.getOperandNumber();
      bool is_dim = producer_operand_number < num_producer_dims;
      auto& replacement_expr = operand_exprs[producer_operand.get()];
      if (!replacement_expr) {
        if (is_dim) {
          int dim_num = producer_operand_number;
          replacement_expr =
              getAffineDimExpr(num_dims + added_dim_args.size(), ctx);
          added_dim_args.push_back(producer_operand.get());
          new_dim_vars.push_back(producer_map.GetDimVars(dim_num));
        } else {
          int sym_num =
              producer_operand_number - producer.getAffineMap().getNumDims();
          replacement_expr =
              getAffineSymbolExpr(num_syms + added_sym_args.size(), ctx);
          added_sym_args.push_back(producer_operand.get());
          new_sym_vars.push_back(producer_map.GetRangeVar(sym_num));
        }
      }
      if (is_dim) {
        producer_dim_replacements.push_back(replacement_expr);
      } else {
        producer_sym_replacements.push_back(replacement_expr);
      }
    }
    replacements[operand_exprs[producer_result]] =
        producer.getAffineMap()
            .getResult(producer_result_id)
            .replaceDimsAndSymbols(producer_dim_replacements,
                                   producer_sym_replacements);
  }

  auto new_affine_map = indexing_map.GetAffineMap().replace(
      replacements, num_dims + added_dim_args.size(),
      num_syms + added_sym_args.size());
  IndexingMap new_indexing_map(new_affine_map, new_dim_vars, new_sym_vars,
                               /*rt_vars=*/{});

  return {new_indexing_map, added_dim_args, added_sym_args};
}
}  // namespace

namespace {

// Simplifies the indexing map.
struct SimplifyIndexingMap : public mlir::OpRewritePattern<ApplyIndexingOp> {
  using OpRewritePattern<ApplyIndexingOp>::OpRewritePattern;

  LogicalResult matchAndRewrite(ApplyIndexingOp indexing_op,
                                PatternRewriter& rewriter) const override {
    IndexingMap indexing_map = indexing_op.getIndexingMap();
    if (!indexing_map.Simplify()) {
      return rewriter.notifyMatchFailure(indexing_op,
                                         "IndexingMap is already simplified");
    }
    rewriter.replaceOpWithNewOp<ApplyIndexingOp>(
        indexing_op, indexing_op.getOperands(), indexing_map);
    return success();
  }
};

// Removes unused variables.
struct RemoveUnusedVariables : public mlir::OpRewritePattern<ApplyIndexingOp> {
  using OpRewritePattern<ApplyIndexingOp>::OpRewritePattern;

  LogicalResult matchAndRewrite(ApplyIndexingOp indexing_op,
                                PatternRewriter& rewriter) const override {
    IndexingMap indexing_map = indexing_op.getIndexingMap();
    auto unused_symbols_bit_vector = indexing_map.RemoveUnusedVars();
    if (unused_symbols_bit_vector.count() == 0) {
      return rewriter.notifyMatchFailure(indexing_op,
                                         "IndexingMap stayed unchanged");
    }
    SmallVector<Value, 4> operands;
    operands.reserve(unused_symbols_bit_vector.count());
    for (int i = 0; i < unused_symbols_bit_vector.size(); ++i) {
      if (!unused_symbols_bit_vector[i]) {
        operands.push_back(indexing_op.getOperand(i));
      }
    }
    rewriter.replaceOpWithNewOp<ApplyIndexingOp>(indexing_op, operands,
                                                 indexing_map);
    return success();
  }
};

struct MoveSymbolsToDims : public mlir::OpRewritePattern<ApplyIndexingOp> {
  using OpRewritePattern<ApplyIndexingOp>::OpRewritePattern;

  LogicalResult matchAndRewrite(ApplyIndexingOp indexing_op,
                                PatternRewriter& rewriter) const override {
    IndexingMap indexing_map = indexing_op.getIndexingMap();
    if (indexing_map.GetSymbolCount() == 0) {
      return rewriter.notifyMatchFailure(indexing_op, "No symbols found");
    }
    rewriter.replaceOpWithNewOp<ApplyIndexingOp>(
        indexing_op, indexing_op->getOperands(),
        indexing_map.ConvertSymbolsToDimensions());
    return success();
  }
};

struct FoldApplyIndexingSequence
    : public mlir::OpRewritePattern<ApplyIndexingOp> {
  using OpRewritePattern<ApplyIndexingOp>::OpRewritePattern;

  LogicalResult matchAndRewrite(ApplyIndexingOp indexing_op,
                                PatternRewriter& rewriter) const override {
    auto indexing_map = indexing_op.getIndexingMap();
    SmallVector<std::pair<int, ApplyIndexingOp>, 2> apply_indexing_ops;
    bool all_apply_indexing_operands_have_one_use = true;
    for (auto& operand : indexing_op->getOpOperands()) {
      if (auto producer = operand.get().getDefiningOp<ApplyIndexingOp>()) {
        apply_indexing_ops.push_back({operand.getOperandNumber(), producer});
        all_apply_indexing_operands_have_one_use &= producer->hasOneUse();
      }
    }
    if (apply_indexing_ops.empty()) {
      return rewriter.notifyMatchFailure(indexing_op,
                                         "No apply_indexing sequences found");
    }
    // If the indexing map has unused variables, we can accidentally fuse an
    // operand that is not used in the map and it can lead to an infinite loop
    // in canonicalizer.
    auto indexing_map_with_no_unused_vars = indexing_map;
    if (indexing_map_with_no_unused_vars.RemoveUnusedVars().count() > 0) {
      indexing_map_with_no_unused_vars.RemoveUnusedVars();
      return rewriter.notifyMatchFailure(indexing_op,
                                         "IndexingMap has unused variables");
    }
    MLIRContext* ctx = indexing_op.getContext();
    int num_dims = indexing_op.getAffineMap().getNumDims();
    int num_syms = indexing_op.getAffineMap().getNumSymbols();
    mlir::DenseMap<Value, AffineExpr> operand_exprs;
    for (auto& operand : indexing_op->getOpOperands()) {
      int operand_number = operand.getOperandNumber();
      operand_exprs[operand.get()] =
          operand_number < num_dims
              ? getAffineDimExpr(operand_number, ctx)
              : getAffineSymbolExpr(operand_number - num_dims, ctx);
    }

    auto replacement = GetNewIndexingMapAfterFoldingSequence(
        indexing_map, apply_indexing_ops, operand_exprs, ctx);

    if (!all_apply_indexing_operands_have_one_use &&
        !replacement.indexing_map.Simplify()) {
      return rewriter.notifyMatchFailure(
          indexing_op, "Folded indexing map was not simplified");
    }

    int new_num_operands = indexing_op->getNumOperands() +
                           replacement.added_dim_args.size() +
                           replacement.added_sym_args.size();
    SmallVector<Value> new_operands;
    new_operands.reserve(new_num_operands);

    auto begin = indexing_op.getOperands().begin();
    new_operands.append(begin, begin + num_dims);
    new_operands.append(replacement.added_dim_args);
    new_operands.append(begin + num_dims, begin + num_dims + num_syms);
    new_operands.append(replacement.added_sym_args);

    rewriter.replaceOpWithNewOp<ApplyIndexingOp>(indexing_op, new_operands,
                                                 replacement.indexing_map);

    return success();
  }
};

// Folds constants into the indexing map.
struct FoldApplyIndexingOperands
    : public mlir::OpRewritePattern<ApplyIndexingOp> {
  using OpRewritePattern<ApplyIndexingOp>::OpRewritePattern;

  LogicalResult matchAndRewrite(ApplyIndexingOp indexing_op,
                                PatternRewriter& rewriter) const override {
    IndexingMap indexing_map = indexing_op.getIndexingMap();
    AffineMap affine_map = indexing_map.GetAffineMap();

    MLIRContext* ctx = affine_map.getContext();
    unsigned num_operands = indexing_op->getNumOperands();
    unsigned num_dims = affine_map.getNumDims();
    unsigned num_symbols = affine_map.getNumSymbols();

    SmallVector<std::optional<int64_t>> constant_values(num_operands,
                                                        std::nullopt);
    int num_constants = 0;
    for (auto& operand : indexing_op->getOpOperands()) {
      if (auto constant =
              operand.get().getDefiningOp<arith::ConstantIndexOp>()) {
        constant_values[operand.getOperandNumber()] = constant.value();
        ++num_constants;
      }
    }
    if (num_constants == 0) {
      return rewriter.notifyMatchFailure(indexing_op,
                                         "No constant operands found");
    }
    SmallVector<AffineExpr, 2> dim_replacements, symbol_replacements;
    dim_replacements.reserve(num_dims);
    symbol_replacements.reserve(num_symbols);

    unsigned new_num_operands = indexing_op->getNumOperands() - num_constants;
    SmallVector<Value, 4> new_operands;
    new_operands.reserve(new_num_operands);
    SmallVector<IndexingMap::Variable, 2> new_dim_vars;
    new_dim_vars.reserve(num_dims);
    SmallVector<IndexingMap::Variable, 2> new_range_vars;
    new_range_vars.reserve(num_symbols);

    unsigned new_num_dims = 0;
    unsigned new_num_symbols = 0;
    for (auto [operand, constant_value] :
         llvm::zip(indexing_op->getOpOperands(), constant_values)) {
      unsigned operand_id = operand.getOperandNumber();
      if (constant_value.has_value()) {
        if (operand_id < num_dims) {
          dim_replacements.push_back(
              getAffineConstantExpr(*constant_value, ctx));
        } else {
          symbol_replacements.push_back(
              getAffineConstantExpr(*constant_value, ctx));
        }
      } else {
        new_operands.push_back(operand.get());
        if (operand_id < num_dims) {
          dim_replacements.push_back(getAffineDimExpr(new_num_dims++, ctx));
          new_dim_vars.push_back(indexing_map.GetDimVars(operand_id));
        } else {
          symbol_replacements.push_back(
              getAffineSymbolExpr(new_num_symbols++, ctx));
          new_range_vars.push_back(
              indexing_map.GetRangeVar(operand_id - num_dims));
        }
      }
    }
    rewriter.replaceOpWithNewOp<ApplyIndexingOp>(
        indexing_op, new_operands,
        affine_map.replaceDimsAndSymbols(dim_replacements, symbol_replacements,
                                         new_num_dims, new_num_symbols),
        new_dim_vars, new_range_vars);
    return success();
  }
};

// Folds constant and dim/symbol expression results.
struct FoldApplyIndexingResults
    : public mlir::OpRewritePattern<ApplyIndexingOp> {
  using OpRewritePattern<ApplyIndexingOp>::OpRewritePattern;

  LogicalResult matchAndRewrite(ApplyIndexingOp indexing_op,
                                PatternRewriter& rewriter) const override {
    Location loc = indexing_op.getLoc();
    IndexingMap indexing_map = indexing_op.getIndexingMap();
    if (indexing_map.IsKnownEmpty()) {
      return rewriter.notifyMatchFailure(indexing_op,
                                         "Domain of the indexing map is empty");
    }
    AffineMap* affine_map = &indexing_map.GetMutableAffineMap();
    unsigned num_results = affine_map->getNumResults();
    SmallVector<AffineExpr, 4> new_exprs;
    new_exprs.reserve(num_results);
    SmallVector<Value, 4> new_values;
    new_values.reserve(num_results);
    for (mlir::OpResult opresult : indexing_op->getOpResults()) {
      if (opresult.use_empty()) {
        new_values.push_back(rewriter.create<arith::ConstantIndexOp>(loc, 0));
        continue;
      }

      unsigned id = opresult.getResultNumber();
      AffineExpr result_expr = affine_map->getResult(id);
      if (auto const_expr =
              mlir::dyn_cast<mlir::AffineConstantExpr>(result_expr)) {
        new_values.push_back(rewriter.create<arith::ConstantIndexOp>(
            loc, const_expr.getValue()));
        continue;
      }
      if (auto dim_expr = mlir::dyn_cast<mlir::AffineDimExpr>(result_expr)) {
        new_values.push_back(indexing_op.getOperand(dim_expr.getPosition()));
        continue;
      }
      if (auto symbol_expr =
              mlir::dyn_cast<mlir::AffineSymbolExpr>(result_expr)) {
        new_values.push_back(indexing_op.getOperand(
            indexing_map.GetDimVarsCount() + symbol_expr.getPosition()));
        continue;
      }
      new_exprs.push_back(result_expr);
      new_values.push_back(Value{});
    }
    if (new_exprs.size() == num_results) {
      return rewriter.notifyMatchFailure(
          indexing_op, "No constant or dim/symbol expression found");
    }
    *affine_map =
        AffineMap::get(affine_map->getNumDims(), affine_map->getNumSymbols(),
                       new_exprs, affine_map->getContext());
    auto new_indexing_op = rewriter.create<ApplyIndexingOp>(
        loc, indexing_op.getOperands(), indexing_map);
    for (int new_result_id = 0, new_indexing_op_result_id = 0;
         new_result_id < new_values.size(); ++new_result_id) {
      auto& new_value = new_values[new_result_id];
      if (new_value) continue;
      new_value = new_indexing_op.getResult(new_indexing_op_result_id++);
    }
    rewriter.replaceOp(indexing_op, new_values);
    return success();
  }
};

}  // namespace

void ApplyIndexingOp::getCanonicalizationPatterns(
    mlir::RewritePatternSet& results, MLIRContext* context) {
  results.add<FoldApplyIndexingOperands, FoldApplyIndexingResults,
              FoldApplyIndexingSequence, MoveSymbolsToDims,
              RemoveUnusedVariables, SimplifyIndexingMap>(context);
}

mlir::LogicalResult ApplyIndexingOp::fold(
    FoldAdaptor adaptor, llvm::SmallVectorImpl<mlir::OpFoldResult>& results) {
  auto map = getAffineMap();
  for (auto expr : map.getResults()) {
    if (auto dim = mlir::dyn_cast<mlir::AffineDimExpr>(expr)) {
      results.push_back(getOperand(dim.getPosition()));
    } else if (auto sym = mlir::dyn_cast<mlir::AffineSymbolExpr>(expr)) {
      results.push_back(getOperand(map.getNumDims() + sym.getPosition()));
    } else {
      results.clear();
      return failure();
    }
  }
  return success();
}

//===----------------------------------------------------------------------===//
// AtomicRMWOp
//===----------------------------------------------------------------------===//

void AtomicRMWOp::getAsmResultNames(
    llvm::function_ref<void(mlir::Value, mlir::StringRef)> setNameFn) {
  setNameFn(getResult(), "atomic_rmw");
}

void AtomicRMWOp::build(OpBuilder& builder, OperationState& result,
                        Value tensor, ValueRange ivs) {
  OpBuilder::InsertionGuard g(builder);
  result.addOperands(tensor);
  result.addOperands(ivs);
  result.addTypes(tensor.getType());

  auto tensor_type = llvm::cast<RankedTensorType>(tensor.getType());
  Region* body = result.addRegion();
  builder.createBlock(body);
  body->addArgument(tensor_type.getElementType(), tensor.getLoc());
}

mlir::OpFoldResult AtomicRMWOp::fold(FoldAdaptor adaptor) {
  auto* body = getBody();
  if (&body->front() == body->getTerminator() &&
      body->front().getOperand(0) == body->getArgument(0)) {
    return getOperand(0);
  }
  return {};
}

//===----------------------------------------------------------------------===//
// PureCallOp
//===----------------------------------------------------------------------===//

void PureCallOp::getAsmResultNames(
    llvm::function_ref<void(mlir::Value, mlir::StringRef)> setNameFn) {
  for (auto result : getResults()) {
    setNameFn(result, "pure_call");
  }
}

//===----------------------------------------------------------------------===//
// SyncThreadsOp
//===----------------------------------------------------------------------===//

void SyncThreadsOp::getAsmResultNames(
    llvm::function_ref<void(mlir::Value, mlir::StringRef)> setNameFn) {
  for (auto result : getResults()) {
    setNameFn(result, "synced_tensor");
  }
}

//===----------------------------------------------------------------------===//
// LoopOp
//===----------------------------------------------------------------------===//

void LoopOp::getAsmResultNames(
    llvm::function_ref<void(mlir::Value, mlir::StringRef)> setNameFn) {
  for (auto result : getResults()) {
    setNameFn(result, "xla_loop");
  }
}

void LoopOp::getAsmBlockArgumentNames(mlir::Region& region,
                                      mlir::OpAsmSetValueNameFn setFn) {
  // i, j, k, l, m, n, n_0, n_1, ...
  char iv_name = 'i';
  for (auto iv : getInductionVars()) {
    setFn(iv, std::string{iv_name});
    if (iv_name <= 'n') {
      ++iv_name;
    }
  }
  // ra, rb, rc, rd, ..., rz, rz_0, ...
  std::string map_result_name = "ra";
  char map_result_char = 'a';
  for (auto map_result : getIndexingMapResults()) {
    setFn(map_result, map_result_name);
    if (map_result_char <= 'z') {
      ++map_result_char;
      map_result_name[1] = map_result_char;
    }
  }
  for (auto iv : getRegionIterArgs()) {
    setFn(iv, "iter");
  }
}

void LoopOp::build(OpBuilder& builder, OperationState& result,
                   IndexingMapAttr indexing_map_attr, ValueRange dims,
                   ValueRange inits, BodyBuilderFn bodyBuilder) {
  OpBuilder::InsertionGuard guard(builder);

  int64_t num_ivs = indexing_map_attr.getRangeVars().size();
  int64_t num_indexing_map_results =
      indexing_map_attr.getIndexingMap().GetNumResults();
  int64_t num_inits = inits.size();
  result.addOperands(dims);
  result.addOperands(inits);
  result.addTypes(TypeRange(inits));
  Block* body_block = builder.createBlock(result.addRegion());
  // Add induction variables and indexing map results block args.
  for (int i = 0, e = num_ivs + num_indexing_map_results; i < e; ++i) {
    body_block->addArgument(builder.getIndexType(), result.location);
  }
  // Add iteration arguments block args.
  for (auto init_type : TypeRange(inits)) {
    body_block->addArguments(init_type, result.location);
  }
  mlir::OperationName opname(LoopOp::getOperationName(), builder.getContext());
  result.addAttribute(LoopOp::getIndexingMapAttrAttrName(opname),
                      indexing_map_attr);
  result.addAttribute(
      LoopOp::getOperandSegmentSizesAttrName(opname),
      builder.getDenseI32ArrayAttr({static_cast<int32_t>(dims.size()),
                                    static_cast<int32_t>(inits.size())}));
  if (bodyBuilder) {
    OpBuilder::InsertionGuard guard(builder);
    builder.setInsertionPointToStart(body_block);
    bodyBuilder(
        builder, result.location,
        body_block->getArguments().take_front(num_ivs),
        body_block->getArguments().drop_front(num_ivs).drop_back(num_inits),
        body_block->getArguments().take_back(num_inits));
  }
}

void LoopOp::build(OpBuilder& builder, OperationState& result,
                   const IndexingMap& indexing_map, ValueRange dims,
                   ValueRange inits, BodyBuilderFn bodyBuilder) {
  build(builder, result,
        IndexingMapAttr::get(builder.getContext(), indexing_map), dims, inits,
        bodyBuilder);
}

ParseResult LoopOp::parse(OpAsmParser& parser, OperationState& result) {
  SmallVector<OpAsmParser::Argument, 4> region_args, ivs, map_results,
      iter_args;
  SmallVector<OpAsmParser::UnresolvedOperand, 4> dim_operands;

  // Parse the dimension values.
  auto* ctx = parser.getContext();
  OpBuilder b(ctx);
  Type index_type = b.getIndexType();
  if (parser.parseOperandList(dim_operands, OpAsmParser::Delimiter::Paren) ||
      parser.resolveOperands(dim_operands, index_type, result.operands))
    return failure();
  // Parse the induction variables.
  if (parser.parseArgumentList(ivs, OpAsmParser::Delimiter::Square))
    return failure();
  for (auto iv : ivs) {
    region_args.push_back(iv);
    region_args.back().type = index_type;
  }

  // Parse the indexing map results variables.
  if (parser.parseArrow() ||
      parser.parseArgumentList(map_results, OpAsmParser::Delimiter::Paren))
    return failure();
  for (auto map_result : map_results) {
    region_args.push_back(map_result);
    region_args.back().type = index_type;
  }

  // Parse the indexing map attribute.
  IndexingMapAttr indexing_map_attr;
  if (parser.parseKeyword("in") ||
      parser.parseAttribute(indexing_map_attr, "indexing_map_attr",
                            result.attributes)) {
    return failure();
  }

  // Parse the arguments.
  SmallVector<OpAsmParser::UnresolvedOperand, 4> init_operands;
  if (parser.parseKeyword("iter_args") ||
      parser.parseAssignmentList(iter_args, init_operands) ||
      parser.parseArrowTypeList(result.types) ||
      parser.resolveOperands(init_operands, result.types, parser.getNameLoc(),
                             result.operands))
    return failure();

  for (auto [index, iter_arg] : llvm::enumerate(iter_args)) {
    region_args.push_back(iter_arg);
    region_args.back().type = result.types[index];
  }

  if (region_args.size() !=
      result.types.size() + ivs.size() + map_results.size()) {
    return parser.emitError(
        parser.getNameLoc(),
        "mismatch in number of induction variables + loop-carried values  + "
        "number of indexing map results variables and the number of results");
  }

  // Parse the body region.
  Region* body = result.addRegion();
  if (parser.parseRegion(*body, region_args)) return failure();
  LoopOp::ensureTerminator(*body, b, result.location);

  // Add the necessary attributes.
  result.addAttribute(
      LoopOp::getOperandSegmentSizeAttr(),
      b.getDenseI32ArrayAttr({static_cast<int32_t>(dim_operands.size()),
                              static_cast<int32_t>(iter_args.size())}));

  // Parse the optional attribute list
  if (parser.parseOptionalAttrDict(result.attributes)) return failure();

  return success();
}

void LoopOp::print(OpAsmPrinter& p) {
  p << " (" << getDims() << ")[" << getInductionVars() << "] -> ("
    << getIndexingMapResults() << ") in " << getIndexingMapAttr()
    << " iter_args(";
  llvm::interleaveComma(
      llvm::zip(getRegionIterArgs(), getInits()), p,
      [&](auto it) { p << std::get<0>(it) << " = " << std::get<1>(it); });
  p << ") -> (" << getInits().getTypes() << ") ";
  p.printRegion(getRegion(), /*printEntryBlockArgs=*/false,
                /*printBlockTerminators=*/true);
  p.printOptionalAttrDict((*this)->getAttrs(),
                          /*elidedAttrs=*/{
                              getIndexingMapAttrAttrName(),
                              getOperandSegmentSizesAttrName(),
                          });
}

LogicalResult LoopOp::verify() {
  if (getInits().size() != getNumResults()) {
    return emitOpError("mismatch in number of loop-carried values and results");
  }
  IndexingMap indexing_map = getIndexingMap();
  if (indexing_map.GetRangeVarsCount() != getNumInductionVars()) {
    return emitOpError() << "mismatch in number of induction variables "
                         << getNumInductionVars()
                         << " and RangeVars in the indexing map "
                         << ToString(indexing_map);
  }
  if (indexing_map.GetDimVarsCount() != getDims().size()) {
    return emitOpError() << "mismatch in number of dims operands "
                         << getDims().size()
                         << " and DimVars in the indexing map "
                         << ToString(indexing_map);
  }
  for (auto [bb_arg, result_type, init] :
       llvm::zip(getRegionIterArgs(), getResultTypes(), getInits())) {
    if (bb_arg.getType() != result_type || init.getType() != result_type) {
      return emitOpError() << "block iter arg type = " << bb_arg.getType()
                           << ", result type = " << result_type
                           << " and init operand type = " << init.getType()
                           << " should match";
    }
  }
  return success();
}

IndexingMap LoopOp::getIndexingMap() {
  return getIndexingMapAttr().getIndexingMap();
}

namespace {
// This pattern is tasked to simplify loop(apply_indexing, ...) patterns to fold
// the apply_indexing ops into the loop op where it can. The pattern assumes
// that the apply_indexing ops have already been simplified via
// MoveSymbolsToDims pattern, which basically means that the producer
// apply_indexing ops should not have any symbols.
struct SimplifyLoopOfApplyIndexing : public mlir::OpRewritePattern<LoopOp> {
  using OpRewritePattern<LoopOp>::OpRewritePattern;

  LogicalResult matchAndRewrite(LoopOp loop_op,
                                PatternRewriter& rewriter) const override {
    auto loop_indexing_map = loop_op.getIndexingMap();
    MLIRContext* ctx = loop_op.getContext();
    int num_dims = loop_indexing_map.GetDimVarsCount();

    SmallVector<std::pair<int, ApplyIndexingOp>, 2> apply_indexing_ops;
    bool all_apply_indexing_operands_have_one_use = true;

    // Only consider dims.
    for (auto& operand : loop_op->getOpOperands().take_front(num_dims)) {
      if (auto producer = operand.get().getDefiningOp<ApplyIndexingOp>()) {
        // Producer should be canonicalized via MoveSymbolsToDims pattern.
        if (producer.getIndexingMap().GetSymbolCount() > 0) {
          continue;
        }
        apply_indexing_ops.push_back({operand.getOperandNumber(), producer});
        all_apply_indexing_operands_have_one_use &= producer->hasOneUse();
      }
    }
    if (apply_indexing_ops.empty()) {
      return rewriter.notifyMatchFailure(
          loop_op,
          "No loop(apply_indexing) patterns found. Note that producer "
          "apply_indexing should have already been simplified via "
          "MoveSymbolsToDims pattern.");
    }

    mlir::DenseMap<Value, AffineExpr> operand_exprs;
    for (auto& operand : loop_op->getOpOperands().take_front(num_dims)) {
      int operand_number = operand.getOperandNumber();
      operand_exprs[operand.get()] = getAffineDimExpr(operand_number, ctx);
    }

    auto replacement = GetNewIndexingMapAfterFoldingSequence(
        loop_indexing_map, apply_indexing_ops, operand_exprs, ctx);

    if (!all_apply_indexing_operands_have_one_use &&
        !replacement.indexing_map.Simplify()) {
      return rewriter.notifyMatchFailure(
          loop_op, "Folded indexing map of the loop op was not simplified");
    }

    int new_num_dims = num_dims + replacement.added_dim_args.size();
    SmallVector<Value> aggregate_dims;
    aggregate_dims.reserve(new_num_dims);

    auto begin = loop_op.getOperands().begin();
    aggregate_dims.append(begin, begin + num_dims);
    aggregate_dims.append(replacement.added_dim_args);

    // Remove unused dims.
    SmallVector<Value, 4> used_dims;
    used_dims.reserve(aggregate_dims.size());
    auto used_dim_bit_vector = ~replacement.indexing_map.RemoveUnusedVars();
    for (auto used_dim_idx : used_dim_bit_vector.set_bits()) {
      if (used_dim_idx < new_num_dims) {
        used_dims.push_back(aggregate_dims[used_dim_idx]);
      }
    }

    auto new_loop_op =
        rewriter.create<LoopOp>(loop_op.getLoc(), replacement.indexing_map,
                                used_dims, loop_op.getInits());
    Block* original_block = &loop_op.getRegion().front();
    Block* new_block = &new_loop_op.getRegion().front();
    rewriter.mergeBlocks(original_block, new_block, new_block->getArguments());
    rewriter.replaceOp(loop_op, new_loop_op.getResults());

    return success();
  }
};
}  // namespace

void LoopOp::getCanonicalizationPatterns(mlir::RewritePatternSet& results,
                                         MLIRContext* context) {
  results.add<SimplifyLoopOfApplyIndexing>(context);
}

//===----------------------------------------------------------------------===//
// MaterializeOp
//===----------------------------------------------------------------------===//

VariableConstraints GetConstraintsForVariables(const IndexingMap& map) {
  VariableConstraints result;
  result.constraints_for_dims.resize(map.GetDimensionCount());
  result.constraints_for_symbols.resize(map.GetSymbolCount());
  for (const auto& constraint : map.GetConstraints()) {
    constraint.first.walk([&](mlir::AffineExpr leaf) {
      if (auto dim = mlir::dyn_cast<mlir::AffineDimExpr>(leaf)) {
        result.constraints_for_dims[dim.getPosition()].insert(constraint);
      } else if (auto sym = mlir::dyn_cast<mlir::AffineSymbolExpr>(leaf)) {
        result.constraints_for_symbols[sym.getPosition()].insert(constraint);
      }
    });
  }
  return result;
}

LogicalResult MaterializeOp::verify() {
  IndexingMap map_in = getMap().getIndexingMap();
  IndexingMap map_out =
      getResult().getType().getIndexingMapAttr().getIndexingMap();
  if (getIndices().size() != map_in.GetDimVarsCount()) {
    return emitOpError() << "number of indices must match number of dimensions "
                            "of indexing map";
  }

  // The thread dimension must have the same domain (range and constraints)
  if (map_in.GetDimVarsCount() == 0 || map_out.GetDimVarsCount() == 0) {
    return emitOpError()
           << "must have thread_id dimension in both indexing maps";
  }
  if (map_in.GetDimVars(0).bounds != map_out.GetDimVars(0).bounds) {
    return emitOpError() << "thread_id dimension must have the same bounds in "
                            "both indexing maps";
  }

  auto variable_constraints_in = GetConstraintsForVariables(map_in);
  auto variable_constraints_out = GetConstraintsForVariables(map_out);
  if (variable_constraints_in.constraints_for_dims[0] !=
      variable_constraints_out.constraints_for_dims[0]) {
    return emitOpError() << "constraints of indexing maps must be equal for "
                         << "the thread_id dimension";
  }

  // The two maps must have the same symbols and they must have the same domain
  if (map_in.GetRangeVarsCount() != map_out.GetRangeVarsCount()) {
    return emitOpError()
           << "number of symbols in both indexing_maps must match";
  }
  for (auto const& [range_in, range_out] :
       llvm::zip(map_in.GetRangeVars(), map_out.GetRangeVars())) {
    if (range_in.bounds != range_out.bounds) {
      return emitOpError() << "domain of symbols of indexing_maps must match";
    }
  }
  if (variable_constraints_in.constraints_for_symbols !=
      variable_constraints_out.constraints_for_symbols) {
    return emitOpError()
           << "constraints of indexing maps must be equal for all symbols";
  }

  // The vector mapping indices must not depend on the block ID
  if (map_out.GetDimVarsCount() > 1) {
    for (auto expr : map_out.GetAffineMap().getResults()) {
      if (expr.isFunctionOfDim(1)) {
        return emitOpError() << "vector mapping indices must not depend on the "
                             << "block ID";
      }
    }
  }
  // If there are constraints on the block ID, they must be the same in both
  // maps
  if (map_in.GetDimVarsCount() > 1 && map_out.GetDimVarsCount() > 1) {
    if (variable_constraints_in.constraints_for_dims[1] !=
        variable_constraints_out.constraints_for_dims[1]) {
      return emitOpError() << "constraints of indexing maps must be equal for "
                           << "the block_id dimension";
    }
  } else if (map_in.GetDimVarsCount() > 1 &&
             !variable_constraints_in.constraints_for_dims[1].empty()) {
    return emitOpError() << "constraints of indexing maps must be equal for "
                         << "the block_id dimension";
  } else if (map_out.GetDimVarsCount() > 1 &&
             !variable_constraints_out.constraints_for_dims[1].empty()) {
    return emitOpError() << "constraints of indexing maps must be equal for "
                         << "the block_id dimension";
  }

  return success();
}

//===----------------------------------------------------------------------===//
// InsertOp
//===----------------------------------------------------------------------===//

LogicalResult InsertOp::verify() {
  if (!getMap().getIndexingMap().GetRangeVars().empty()) {
    return emitOpError() << "insert_op map must not have any symbols";
  }
  int64_t vector_map_num_results =
      getSource().getType().getIndexingMapAttr().getNumResults();
  if (vector_map_num_results != getMap().getIndexingMap().GetDimVars().size()) {
    return emitOpError() << "source map result count must equal insert_op's "
                            "map's dimension count";
  }
  return success();
}

//===----------------------------------------------------------------------===//
// ReindexOp
//===----------------------------------------------------------------------===//

void ReindexOp::build(mlir::OpBuilder& builder, mlir::OperationState& result,
                      mlir::Type type, mlir::Value operand, mlir::Value padding,
                      const xla::gpu::IndexingMap& indexing_map) {
  IndexingMapAttr indexing_map_attr =
      IndexingMapAttr::get(builder.getContext(), indexing_map);
  build(builder, result, type, operand, padding, indexing_map_attr);
}

//===----------------------------------------------------------------------===//
// ReduceOp
//===----------------------------------------------------------------------===//

SmallVector<Type, 2> inferReductionResultTypes(TypeRange input_types,
                                               ArrayRef<int64_t> reduced_dims) {
  auto input_shape =
      mlir::cast<RankedTensorType>(input_types.front()).getShape();
  auto num_reduced_dims = reduced_dims.size();
  SmallVector<int64_t, 4> output_shape;
  output_shape.reserve(input_shape.size() - num_reduced_dims);
  int reduce_dim = 0;
  for (int64_t i = 0; i < input_shape.size(); ++i) {
    if (reduce_dim < num_reduced_dims && i == reduced_dims[reduce_dim]) {
      ++reduce_dim;
      continue;
    }
    output_shape.push_back(input_shape[i]);
  }
  SmallVector<Type, 2> result_types;
  result_types.reserve(input_types.size());
  for (auto input_type : input_types) {
    result_types.push_back(RankedTensorType::get(
        output_shape,
        mlir::cast<RankedTensorType>(input_type).getElementType()));
  }
  return result_types;
}

SmallVector<Type, 2> inferReductionInitTypes(TypeRange input_types) {
  SmallVector<Type, 2> init_types;
  init_types.reserve(input_types.size());
  for (auto input_type : input_types) {
    init_types.push_back(
        mlir::cast<RankedTensorType>(input_type).getElementType());
  }
  return init_types;
}

LogicalResult ReduceOp::inferReturnTypes(
    MLIRContext* context, std::optional<Location> location, ValueRange operands,
    mlir::DictionaryAttr attributes, mlir::OpaqueProperties properties,
    mlir::RegionRange regions,
    mlir::SmallVectorImpl<Type>& inferredReturnTypes) {
  ReduceOp::Adaptor adaptor(operands, attributes, properties, regions);
  inferredReturnTypes.append(inferReductionResultTypes(
      TypeRange{adaptor.getInputs()}, adaptor.getDimensions()));
  return success();
}

ParseResult ReduceOp::parse(OpAsmParser& parser, OperationState& result) {
  SmallVector<OpAsmParser::UnresolvedOperand, 4> inputs;
  SmallVector<OpAsmParser::UnresolvedOperand, 4> inits;
  SmallVector<int64_t, 2> dimensions;
  mlir::StringAttr combiner;
  SmallVector<Type, 2> input_types;
  SmallVector<Type, 2> result_types;

  if (parser.parseLParen() || parseOperands(parser, &inputs) ||
      parser.parseRParen() || parser.parseKeyword("inits") ||
      parser.parseLParen() || parseOperands(parser, &inits) ||
      parser.parseRParen() || parser.parseKeyword("dimensions") ||
      parser.parseEqual() ||
      parser.parseCommaSeparatedList(OpAsmParser::Delimiter::Square,
                                     [&]() -> ParseResult {
                                       return parser.parseInteger(
                                           dimensions.emplace_back());
                                     }) ||
      parser.parseKeyword("combiner") || parser.parseEqual() ||
      parser.parseSymbolName(combiner) ||
      parser.parseOptionalAttrDict(result.attributes) ||
      parser.parseColonTypeList(input_types) || parser.parseKeyword("to") ||
      parser.parseTypeList(result_types)) {
    return failure();
  }
  auto ctx = result.getContext();
  mlir::OperationName opname(ReduceOp::getOperationName(), ctx);
  result.addAttribute(ReduceOp::getDimensionsAttrName(opname),
                      DenseI64ArrayAttr::get(ctx, dimensions));
  result.addAttribute(ReduceOp::getCombinerAttrName(opname),
                      mlir::FlatSymbolRefAttr::get(ctx, combiner));
  result.addTypes(result_types);

  auto init_types = inferReductionInitTypes(input_types);
  mlir::SMLoc loc = parser.getCurrentLocation();
  if (parser.resolveOperands(inputs, input_types, loc, result.operands) ||
      parser.resolveOperands(inits, init_types, loc, result.operands)) {
    return failure();
  }
  return success();
}

void ReduceOp::print(OpAsmPrinter& p) {
  p << '(' << getInputs() << ") inits(" << getInits() << ") dimensions=["
    << getDimensions() << "] combiner=@" << getCombiner();
  p.printOptionalAttrDict((*this)->getAttrs(),
                          {getCombinerAttrName(), getDimensionsAttrName()});
  p << " : " << TypeRange(getInputs()) << " to " << TypeRange(getResults());
}

LogicalResult ReduceOp::verify() {
  // Check init types.
  auto inferred_init_types = inferReductionInitTypes(TypeRange(getInputs()));
  for (auto [inferred_init_type, init_type] :
       llvm::zip(inferred_init_types, TypeRange(getInits()))) {
    if (inferred_init_type != init_type) {
      return emitOpError() << "init type " << init_type
                           << " does not match inferred type "
                           << inferred_init_type;
    }
  }
  // Check combiner.
  auto module = this->getOperation()->getParentOfType<mlir::ModuleOp>();
  auto combiner = module.lookupSymbol<mlir::func::FuncOp>(getCombinerAttr());
  if (!combiner) {
    return emitOpError() << "combiner `@" << getCombiner() << "` not found";
  }
  SmallVector<Type, 2> combiner_operand_types;
  combiner_operand_types.reserve(getNumOperands());
  combiner_operand_types.append(inferred_init_types);
  combiner_operand_types.append(inferred_init_types);
  auto expected_combiner_type = mlir::FunctionType::get(
      getContext(), combiner_operand_types, inferred_init_types);
  if (expected_combiner_type != combiner.getFunctionType()) {
    return emitOpError() << "provided combiner `@" << getCombiner()
                         << " expected to have type " << expected_combiner_type
                         << " but got " << combiner.getFunctionType();
  }
  return success();
}

//===----------------------------------------------------------------------===//
// ShuffleReduceOp
//===----------------------------------------------------------------------===//

ParseResult ShuffleReduceOp::parse(OpAsmParser& parser,
                                   OperationState& result) {
  SmallVector<OpAsmParser::UnresolvedOperand, 4> inputs;
  mlir::StringAttr combiner;
  int64_t max_distance;
  SmallVector<Type, 2> operand_types;
  mlir::SMLoc loc = parser.getCurrentLocation();
  if (parser.parseLParen() || parseOperands(parser, &inputs) ||
      parser.parseRParen() || parser.parseKeyword("to") ||
      parser.parseInteger(max_distance) || parser.parseKeyword("combiner") ||
      parser.parseEqual() || parser.parseSymbolName(combiner) ||
      parser.parseOptionalAttrDict(result.attributes) ||
      parser.parseColonTypeList(operand_types) ||
      parser.resolveOperands(inputs, operand_types, loc, result.operands)) {
    return failure();
  }
  auto ctx = result.getContext();
  mlir::OperationName opname(ShuffleReduceOp::getOperationName(), ctx);
  result.addAttribute(ShuffleReduceOp::getCombinerAttrName(opname),
                      mlir::FlatSymbolRefAttr::get(ctx, combiner));
  result.addAttribute(
      ShuffleReduceOp::getMaxDistanceAttrName(opname),
      mlir::IntegerAttr::get(mlir::IntegerType::get(ctx, 64), max_distance));
  result.addTypes(operand_types);
  return success();
}

void ShuffleReduceOp::print(OpAsmPrinter& p) {
  p << '(' << getOperands() << ") to " << getMaxDistance() << " combiner=@"
    << getCombiner();
  p.printOptionalAttrDict((*this)->getAttrs(),
                          {getCombinerAttrName(), getMaxDistanceAttrName()});
  p << " : " << TypeRange(getResultTypes());
}

}  // namespace gpu
}  // namespace xla

#define GET_OP_CLASSES
#include "xla/service/gpu/fusions/ir/xla_gpu_ops.cc.inc"
