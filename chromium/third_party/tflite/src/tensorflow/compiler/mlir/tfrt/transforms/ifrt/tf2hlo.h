/* Copyright 2023 The TensorFlow Authors. All Rights Reserved.

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

#ifndef TENSORFLOW_COMPILER_MLIR_TFRT_TRANSFORMS_IFRT_TF2HLO_H_
#define TENSORFLOW_COMPILER_MLIR_TFRT_TRANSFORMS_IFRT_TF2HLO_H_

#include <memory>
#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "mlir/IR/BuiltinOps.h"  // from @llvm-project
#include "mlir/IR/OwningOpRef.h"  // from @llvm-project
#include "tensorflow/compiler/mlir/tfrt/transforms/ifrt/ifrt_compilation.pb.h"
#include "tensorflow/compiler/mlir/tfrt/transforms/ifrt/ifrt_types.h"
#include "tensorflow/compiler/tf2xla/xla_helpers.h"
#include "xla/client/compile_only_client.h"
#include "xla/python/ifrt/client.h"
#include "xla/python/ifrt/device_list.h"
#include "xla/python/ifrt/topology.h"
#include "xla/service/hlo.pb.h"
#include "xla/tsl/concurrency/ref_count.h"
#include "tensorflow/core/framework/types.pb.h"
#include "tensorflow/core/protobuf/tpu/compile_metadata.pb.h"

namespace tensorflow {
namespace ifrt_serving {

struct Tf2HloArg {
  mlir::ModuleOp module;
  absl::Span<const DtypeAndShape> input_dtypes_and_shapes;
  absl::string_view entry_function_name;
  tensorflow::tpu::TPUCompileMetadataProto compile_metadata;
  tensorflow::XlaHelpers::ShapeRepresentationFn shape_representation_fn;
  std::shared_ptr<xla::ifrt::Topology> topology;
  absl::string_view platform_name;

  absl::StatusOr<std::string> Key();
};

struct Tf2HloResult {
  xla::HloModuleProto hlo_module_proto;
  tensorflow::tpu::TPUCompileMetadataProto compile_metadata;
  tf2xla::HostComputeMetadata host_compute_metadata;
  Tf2HLOResultProto ToProto() const;
};

absl::Status UpdateCompileMetadata(
    tensorflow::tpu::TPUCompileMetadataProto& metadata,
    absl::Span<const DtypeAndShape> inputs);

absl::StatusOr<tensorflow::tpu::TPUCompileMetadataProto> GetCompileMetadata(
    mlir::ModuleOp module, const xla::ifrt::Client& ifrt_client);

// A class that convert tf module to hlo
absl::StatusOr<Tf2HloResult> CompileTfToHlo(const Tf2HloArg& arg);

}  // namespace ifrt_serving
}  // namespace tensorflow

#endif  // TENSORFLOW_COMPILER_MLIR_TFRT_TRANSFORMS_IFRT_TF2HLO_H_
