/* Copyright 2020 The TensorFlow Authors. All Rights Reserved.

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
#ifndef TENSORFLOW_COMPILER_MLIR_LITE_PYTHON_TF_TFL_FLATBUFFER_HELPERS_H_
#define TENSORFLOW_COMPILER_MLIR_LITE_PYTHON_TF_TFL_FLATBUFFER_HELPERS_H_

#include <optional>
#include <ostream>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "mlir/Dialect/Func/IR/FuncOps.h"  // from @llvm-project
#include "mlir/IR/BuiltinOps.h"  // from @llvm-project
#include "mlir/IR/OwningOpRef.h"  // from @llvm-project
#include "tensorflow/cc/saved_model/loader.h"
#include "tensorflow/compiler/mlir/lite/common/tfl_pass_config.h"
#include "tensorflow/compiler/mlir/lite/transforms/passes.h"
#include "tensorflow/compiler/mlir/quantization/common/quantization_lib/quantization_config.h"
#include "tensorflow/compiler/mlir/quantization/tensorflow/python/py_function_lib.h"
#include "tensorflow/core/platform/status.h"
#include "tensorflow/core/platform/types.h"
#include "tensorflow/core/public/session.h"
#include "tensorflow/lite/toco/model_flags.pb.h"
#include "tensorflow/lite/toco/toco_flags.pb.h"
#include "tensorflow/lite/toco/types.pb.h"
#include "tsl/platform/statusor.h"

namespace tensorflow {
namespace internal {

// Register all custom ops including user specified custom ops.
Status RegisterAllCustomOps(const toco::TocoFlags& toco_flags);

// Populate quantization specs (or not) given user specified ranges for each
// input arrays.
Status PopulateQuantizationSpecs(
    const toco::ModelFlags& model_flags, toco::TocoFlags& toco_flags,
    mlir::quant::QuantizationSpecs* quant_specs,
    std::vector<string>* node_names, std::vector<string>* node_dtypes,
    std::vector<std::optional<std::vector<int>>>* node_shapes,
    std::vector<std::optional<double>>* node_mins,
    std::vector<std::optional<double>>* node_maxs);

// Convert imported MLIR file to TfLite flatbuffer.
// This will also run relevant passes as well.
Status ConvertMLIRToTFLiteFlatBuffer(
    const toco::ModelFlags& model_flags, toco::TocoFlags& toco_flags,
    std::unique_ptr<mlir::MLIRContext>&& context,
    mlir::OwningOpRef<mlir::ModuleOp> module,
    const mlir::TFL::PassConfig& pass_config,
    const std::unordered_set<std::string>& saved_model_tags, string* result,
    std::unique_ptr<SavedModelBundle>&& saved_model_bundle,
    const quantization::PyFunctionLibrary* quantization_py_function_lib);

// Give a warning for any unused flags that have been specified.
void WarningUnusedFlags(const toco::ModelFlags& model_flags,
                        const toco::TocoFlags& toco_flags);
}  // namespace internal
}  // namespace tensorflow

#endif  // TENSORFLOW_COMPILER_MLIR_LITE_PYTHON_TF_TFL_FLATBUFFER_HELPERS_H_
