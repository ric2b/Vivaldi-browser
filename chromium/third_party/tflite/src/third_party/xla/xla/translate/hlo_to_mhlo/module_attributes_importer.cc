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

#include "xla/translate/hlo_to_mhlo/module_attributes_importer.h"

#include <cstdint>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/types/span.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallVector.h"
#include "mlir/IR/AffineMap.h"
#include "mlir/IR/Attributes.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/BuiltinOps.h"
#include "xla/hlo/ir/hlo_computation.h"
#include "xla/hlo/ir/hlo_module.h"
#include "xla/hlo/ir/hlo_sharding.h"
#include "xla/layout.h"
#include "xla/mlir_hlo/mhlo/IR/hlo_ops.h"
#include "xla/service/computation_layout.h"
#include "xla/service/hlo_module_config.h"
#include "xla/shape.h"
#include "xla/shape_layout.h"
#include "xla/shape_util.h"
#include "xla/translate/hlo_to_mhlo/hlo_function_importer.h"
#include "xla/translate/hlo_to_mhlo/hlo_utils.h"
#include "xla/util.h"
#include "xla/xla_data.pb.h"

namespace xla {
namespace {

constexpr char kCrossProgramPrefetches[] = "mhlo.cross_program_prefetches";
constexpr char kEntryComputationParameterLayouts[] =
    "mhlo.xla_entry_computation_parameter_layouts";
constexpr char kEntryComputationParameterTiles[] =
    "mhlo.xla_entry_computation_parameter_tiles";
constexpr char kEntryComputationResultLayout[] =
    "mhlo.xla_entry_computation_result_layout";
constexpr char kEntryComputationResultTiles[] =
    "mhlo.xla_entry_computation_result_tiles";
constexpr char kFrontendAttributes[] = "mhlo.frontend_attributes";
constexpr char kInputOutputAlias[] = "mhlo.input_output_alias";
constexpr char kIsDynamic[] = "mhlo.is_dynamic";
constexpr char kNumPartitions[] = "mhlo.num_partitions";
constexpr char kNumReplicas[] = "mhlo.num_replicas";
constexpr char kSpmdOutputSharding[] = "mhlo.spmd_output_sharding";
constexpr char kSpmdParametersShardings[] = "mhlo.spmd_parameters_shardings";
constexpr char kUseAutoSpmdPartitioning[] = "mhlo.use_auto_spmd_partitioning";

mlir::ArrayAttr ConvertCrossProgramPrefetches(
    const absl::Span<const xla::HloModule::CrossProgramPrefetchInfo> prefetches,
    const xla::HloComputation& entryComputation, mlir::Builder* builder,
    bool flatten_computation_args_result) {
  llvm::SmallVector<mlir::Attribute, 4> shapes;
  shapes.reserve(prefetches.size());
  if (flatten_computation_args_result) {
    llvm::SmallVector<absl::flat_hash_map<xla::ShapeIndex, int64_t>>
        original_param_index_to_flattened_arg_index;
    int64_t arg_index = 0;
    for (xla::HloInstruction* param_instruction :
         entryComputation.parameter_instructions()) {
      auto& param_map =
          original_param_index_to_flattened_arg_index.emplace_back();
      xla::ShapeUtil::ForEachLeafShape(
          param_instruction->shape(),
          [&](const xla::Shape&, const xla::ShapeIndex& index) {
            param_map[index] = arg_index++;
          });
    }
    for (const auto& [parameter, index, alt_memory_offset] : prefetches)
      shapes.push_back(mlir::mhlo::CrossProgramPrefetchAttr::get(
          builder->getContext(),
          original_param_index_to_flattened_arg_index[parameter][index],
          /*indices=*/{}, alt_memory_offset));
  } else {
    for (const auto& [parameter, index, alt_memory_offset] : prefetches)
      shapes.push_back(mlir::mhlo::CrossProgramPrefetchAttr::get(
          builder->getContext(), parameter,
          llvm::ArrayRef<int64_t>(index.data(), index.size()),
          alt_memory_offset));
  }

  return mlir::ArrayAttr::get(builder->getContext(), shapes);
}

void ImportEntryComputationParameterLayoutAndTiles(
    const xla::HloModule& hlo_module, mlir::ModuleOp module,
    const ComputationLayout& computation_layout, mlir::Builder builder) {
  llvm::SmallVector<mlir::Attribute> parameter_layouts;
  llvm::SmallVector<mlir::Attribute> parameter_tiles;
  for (auto& layout : computation_layout.parameter_layouts()) {
    if (layout.shape().IsTuple()) {
      llvm::SmallVector<mlir::Attribute> tuple_element_parameter_layouts;
      llvm::SmallVector<mlir::Attribute> tuple_element_parameter_tiles;
      for (auto& tuple_element_shape : layout.shape().tuple_shapes()) {
        std::pair<mlir::Attribute, mlir::Attribute> layout_attrs =
            GetLayoutAttribute(builder, tuple_element_shape);
        tuple_element_parameter_layouts.push_back(layout_attrs.first);
        tuple_element_parameter_tiles.push_back(layout_attrs.second);
      }
      parameter_layouts.push_back(
          builder.getArrayAttr({tuple_element_parameter_layouts}));
      parameter_tiles.push_back(
          builder.getArrayAttr({tuple_element_parameter_tiles}));
    } else {
      std::pair<mlir::Attribute, mlir::ArrayAttr> layout_attrs =
          GetLayoutAttribute(builder, layout.shape());
      parameter_layouts.push_back(layout_attrs.first);
      parameter_tiles.push_back(layout_attrs.second);
    }
  }
  module->setAttr(kEntryComputationParameterLayouts,
                  builder.getArrayAttr({parameter_layouts}));
  module->setAttr(kEntryComputationParameterTiles,
                  builder.getArrayAttr({parameter_tiles}));
}

void ImportEntryComputationResultLayoutAndTiles(
    const xla::HloModule& hlo_module, mlir::ModuleOp module,
    const ComputationLayout& computation_layout, mlir::Builder builder) {
  if (computation_layout.result_layout().shape().IsTuple()) {
    llvm::SmallVector<mlir::Attribute> result_layouts;
    llvm::SmallVector<mlir::Attribute> result_tiles;
    for (auto& tuple_element_layout :
         computation_layout.result_layout().shape().tuple_shapes()) {
      std::pair<mlir::Attribute, mlir::Attribute> layout_attrs =
          GetLayoutAttribute(builder, tuple_element_layout);
      result_layouts.push_back(layout_attrs.first);
      result_tiles.push_back(layout_attrs.second);
    }
    module->setAttr(
        kEntryComputationResultLayout,
        builder.getArrayAttr({builder.getArrayAttr(result_layouts)}));
    module->setAttr(kEntryComputationResultTiles,
                    builder.getArrayAttr({builder.getArrayAttr(result_tiles)}));
  } else {
    std::pair<mlir::Attribute, mlir::ArrayAttr> layout_attrs =
        GetLayoutAttribute(builder, computation_layout.result_layout().shape(),
                           computation_layout.result_layout().layout());
    module->setAttr(kEntryComputationResultLayout,
                    builder.getArrayAttr({layout_attrs.first}));
    module->setAttr(kEntryComputationResultTiles,
                    builder.getArrayAttr({layout_attrs.second}));
  }
}

}  // namespace

void ImportCrossProgramPrefetches(const xla::HloModule& hlo_module,
                                  mlir::ModuleOp module,
                                  bool flatten_computation_args_result,
                                  mlir::Builder builder) {
  module->setAttr(
      kCrossProgramPrefetches,
      ConvertCrossProgramPrefetches(hlo_module.CrossProgramPrefetches(),
                                    *hlo_module.entry_computation(), &builder,
                                    flatten_computation_args_result));
}

void ImportEntryComputationLayoutAndTiles(const xla::HloModule& hlo_module,
                                          mlir::ModuleOp module,
                                          mlir::Builder builder) {
  const auto& computation_layout = hlo_module.entry_computation_layout();
  if (!computation_layout.LayoutIsSet()) return;

  // The MLIR CPU pipeline assumes default layouts throughout the program. At
  // the boundaries, this may not be the case, so layout information needs to
  // be propagated to adapt the data layouts.
  if (llvm::any_of(computation_layout.parameter_layouts(),
                   [](const ShapeLayout& shape) {
                     return HasCustomLayout(shape.shape());
                   })) {
    ImportEntryComputationParameterLayoutAndTiles(hlo_module, module,
                                                  computation_layout, builder);
  }
  if (HasCustomLayout(computation_layout.result_layout().shape())) {
    ImportEntryComputationResultLayoutAndTiles(hlo_module, module,
                                               computation_layout, builder);
  }
}

void ImportFrontendAttributes(const xla::HloModule& hlo_module,
                              mlir::ModuleOp module, mlir::Builder builder) {
  if (!hlo_module.frontend_attributes().map().empty()) {
    llvm::SmallVector<mlir::NamedAttribute, 4> frontend_attributes;
    for (const auto& [k, v] : hlo_module.frontend_attributes().map())
      frontend_attributes.push_back(
          builder.getNamedAttr(k, builder.getStringAttr(v)));
    if (!frontend_attributes.empty())
      module->setAttr(kFrontendAttributes,
                      builder.getDictionaryAttr(frontend_attributes));
  }
}

void ImportNumPartitions(const xla::HloModule& hlo_module,
                         mlir::ModuleOp module, mlir::Builder builder) {
  const auto& config = hlo_module.config();
  if (config.num_partitions() != 1) {
    module->setAttr(kNumPartitions,
                    builder.getI32IntegerAttr(config.num_partitions()));
  }
}

void ImportNumReplicas(const xla::HloModule& hlo_module, mlir::ModuleOp module,
                       mlir::Builder builder) {
  const auto& config = hlo_module.config();
  if (config.replica_count() != 1) {
    module->setAttr(kNumReplicas,
                    builder.getI32IntegerAttr(config.replica_count()));
  }
}

void ImportInputOutputAlias(const xla::HloModule& hlo_module,
                            mlir::ModuleOp module, mlir::Builder builder) {
  module->setAttr(kInputOutputAlias,
                  ConvertInputOutputAlias(
                      hlo_module.input_output_alias_config(), &builder));
}

void ImportIsDynamic(const xla::HloModule& hlo_module, mlir::ModuleOp module,
                     mlir::Builder builder) {
  module->setAttr(kIsDynamic, mlir::BoolAttr::get(builder.getContext(),
                                                  hlo_module.is_dynamic()));
}

void ImportSpmdOutputSharding(const xla::HloModule& hlo_module,
                              mlir::ModuleOp module, mlir::Builder builder) {
  if (hlo_module.has_spmd_output_sharding())
    module->setAttr(
        kSpmdOutputSharding,
        ConvertSharding(hlo_module.spmd_output_sharding(), &builder));
}

void ImportSpmdParametersShardings(const xla::HloModule& hlo_module,
                                   mlir::ModuleOp module,
                                   bool flatten_computation_args_result,
                                   mlir::Builder builder) {
  if (hlo_module.has_spmd_parameters_shardings()) {
    llvm::SmallVector<mlir::Attribute> parameter_shardings;
    parameter_shardings.reserve(hlo_module.spmd_parameters_shardings().size());
    for (const auto& root_sharding : hlo_module.spmd_parameters_shardings()) {
      llvm::ArrayRef<HloSharding> shardings = root_sharding;
      if (root_sharding.IsTuple() && flatten_computation_args_result)
        shardings = root_sharding.tuple_elements();
      for (const auto& sharding : shardings)
        parameter_shardings.push_back(ConvertSharding(sharding, &builder));
    }
    module->setAttr(kSpmdParametersShardings,
                    builder.getArrayAttr(parameter_shardings));
  }
}

void ImportUseAutoSpmdPartitioning(const xla::HloModule& hlo_module,
                                   mlir::ModuleOp module,
                                   mlir::Builder builder) {
  module->setAttr(kUseAutoSpmdPartitioning,
                  mlir::BoolAttr::get(builder.getContext(),
                                      hlo_module.use_auto_spmd_partitioning()));
}

}  // namespace xla
