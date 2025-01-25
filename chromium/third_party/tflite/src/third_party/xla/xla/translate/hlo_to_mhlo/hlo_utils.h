/* Copyright 2019 The OpenXLA Authors.

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

// This file defines helpers useful when creating or manipulating lhlo/hlo.

#ifndef XLA_TRANSLATE_HLO_TO_MHLO_HLO_UTILS_H_
#define XLA_TRANSLATE_HLO_TO_MHLO_HLO_UTILS_H_

#include "llvm/ADT/STLExtras.h"
#include "mlir/Dialect/SparseTensor/IR/Enums.h"  // from @llvm-project
#include "mlir/Dialect/SparseTensor/IR/SparseTensor.h"  // from @llvm-project
#include "mlir/IR/Attributes.h"  // from @llvm-project
#include "mlir/IR/Builders.h"  // from @llvm-project
#include "mlir/IR/BuiltinTypes.h"  // from @llvm-project
#include "xla/hlo/ir/hlo_instruction.h"
#include "xla/mlir/utils/type_util.h"
#include "xla/mlir_hlo/mhlo/IR/hlo_ops.h"
#include "xla/mlir_hlo/utils/convert_op_folder.h"
#include "xla/util.h"

namespace xla {

absl::StatusOr<mlir::DenseElementsAttr> CreateDenseElementsAttrFromLiteral(
    const LiteralBase& literal, mlir::Builder builder);

// Creates an DenseIntElementsAttr using the elements of the vector and the
// optional shape.
mlir::DenseIntElementsAttr CreateDenseIntElementsAttrFromVector(
    const llvm::ArrayRef<int64_t> vector, mlir::Builder builder,
    llvm::ArrayRef<int64_t> shape = {});

// Converts the given XLA shape for tensors to the template MLIR type.
template <typename TypeT>
static absl::StatusOr<TypeT> ConvertTensorShapeToType(const Shape& xla_ty,
                                                      mlir::Builder builder) {
  auto element_type_or =
      ConvertPrimitiveTypeToMlirType(xla_ty.element_type(), builder);
  if (!element_type_or.ok()) return element_type_or.status();

  bool is_bounded_dynamic = false;
  int64_t rank = xla_ty.rank();
  llvm::SmallVector<int64_t, 4> shape(rank, mlir::ShapedType::kDynamic);
  llvm::SmallVector<int64_t, 4> bounds(rank, mlir::ShapedType::kDynamic);
  for (int64_t dim = 0; dim < rank; ++dim) {
    int64_t dim_size = xla_ty.dimensions(dim);
    if (xla_ty.is_dynamic_dimension(dim)) {
      if (!xla_ty.is_unbounded_dynamic_dimension(dim)) {
        bounds[dim] = dim_size;
        is_bounded_dynamic = true;
      }
    } else {
      shape[dim] = dim_size;
    }
  }
  using mlir::mhlo::TypeExtensionsAttr;
  mlir::Attribute encoding;
  if (is_bounded_dynamic) {
    encoding = TypeExtensionsAttr::get(builder.getContext(), bounds);
  }

  using mlir::sparse_tensor::SparseTensorEncodingAttr;
  // TODO(b/238903065): We don't yet support bounded dynamism shapes and
  // sparsity at the same time, as we can currently only have one `encoding` on
  // a RankedTensorType, and we don't currently have a meet of
  // SparseTensorEncodingAttr and TypeExtensionsAttr (which holds bounds).
  //
  // For example, we wouldn't be able to represent the xla type
  // `f32[4,<=4]{1,0:D(D,C)}`.
  if (xla_ty.has_layout()) {
    auto layout = xla_ty.layout();
    if (LayoutUtil::IsSparse(layout)) {
      if (is_bounded_dynamic)
        return Unimplemented(
            "MHLO doesn't support bounded dynamic shapes for sparse tensors");
      llvm::SmallVector<mlir::sparse_tensor::LevelType> lts;
      for (size_t i = 0, e = layout.dim_level_types_size(); i < e; ++i) {
        auto dlt = layout.dim_level_type(i);
        bool ordered =
            i < layout.dim_ordered_size() ? layout.dim_ordered(i) : true;
        bool unique =
            i < layout.dim_unique_size() ? layout.dim_unique(i) : true;
        switch (dlt) {
          case DimLevelType::DIM_DENSE:
            lts.push_back(*mlir::sparse_tensor::buildLevelType(
                mlir::sparse_tensor::LevelFormat::Dense, ordered, unique));
            break;
          case DimLevelType::DIM_COMPRESSED:
            lts.push_back(*mlir::sparse_tensor::buildLevelType(
                mlir::sparse_tensor::LevelFormat::Compressed, ordered, unique));
            break;
          case DimLevelType::DIM_SINGLETON:
            lts.push_back(*mlir::sparse_tensor::buildLevelType(
                mlir::sparse_tensor::LevelFormat::Singleton, ordered, unique));
            break;
          case DimLevelType::DIM_LOOSE_COMPRESSED:
            lts.push_back(*mlir::sparse_tensor::buildLevelType(
                mlir::sparse_tensor::LevelFormat::LooseCompressed, ordered,
                unique));
            break;
          default:
            return InvalidArgument("Unknown DimLevelType from HLO");
        }
      }
      auto ordering = layout.minor_to_major();
      llvm::SmallVector<uint32_t> major_to_minor = {ordering.rbegin(),
                                                    ordering.rend()};
      auto id_map = mlir::AffineMap::getPermutationMap(major_to_minor,
                                                       builder.getContext());
      // TODO(atondwal): support sizes other than 32 when XLA does
      encoding = SparseTensorEncodingAttr::get(
          builder.getContext(), lts, id_map, mlir::AffineMap(), 32, 32);
    }
  }
  return TypeT::get(shape, element_type_or.value(), encoding);
}

absl::StatusOr<mlir::MemRefType> ConvertTensorShapeToMemRefType(
    const Shape& shape, mlir::Builder builder);

template <>
inline absl::StatusOr<mlir::MemRefType> ConvertTensorShapeToType(
    const Shape& shape, mlir::Builder builder) {
  if (shape.is_dynamic()) {
    return FailedPrecondition(  // NOLINT
        "MemRefType don't support dynamic shapes");
  }
  return ConvertTensorShapeToMemRefType(shape, builder);
}

// Converts the given XLA shape to the template MLIR type.
template <typename TypeT>
static absl::StatusOr<mlir::Type> ConvertShapeToType(const Shape& shape,
                                                     mlir::Builder builder) {
  if (shape.IsTuple()) {
    llvm::SmallVector<mlir::Type, 4> contents;
    contents.reserve(shape.tuple_shapes_size());
    for (const auto& subtype : shape.tuple_shapes()) {
      TF_ASSIGN_OR_RETURN(auto mlir_subtype,
                          ConvertShapeToType<TypeT>(subtype, builder));
      contents.push_back(mlir_subtype);
    }
    return builder.getTupleType(contents);
  }
  if (shape.IsToken()) {
    return mlir::mhlo::TokenType::get(builder.getContext());
  }
  return ConvertTensorShapeToType<TypeT>(shape, builder);
}

}  // namespace xla

#endif  // XLA_TRANSLATE_HLO_TO_MHLO_HLO_UTILS_H_
