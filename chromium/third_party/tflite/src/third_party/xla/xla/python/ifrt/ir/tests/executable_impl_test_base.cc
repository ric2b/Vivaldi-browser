/* Copyright 2023 The OpenXLA Authors.

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

#include "xla/python/ifrt/ir/tests/executable_impl_test_base.h"

#include <memory>
#include <optional>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/DialectRegistry.h"
#include "mlir/IR/MLIRContext.h"
#include "mlir/IR/OwningOpRef.h"
#include "mlir/InitAllDialects.h"
#include "mlir/Parser/Parser.h"
#include "xla/mlir/utils/error_util.h"
#include "xla/mlir_hlo/mhlo/IR/register.h"
#include "xla/python/ifrt/array.h"
#include "xla/python/ifrt/device.h"
#include "xla/python/ifrt/device_list.h"
#include "xla/python/ifrt/dtype.h"
#include "xla/python/ifrt/ir/ifrt_dialect.h"
#include "xla/python/ifrt/ir/sharding_param.h"
#include "xla/python/ifrt/ir/transforms/built_in_spmd_expansions.h"
#include "xla/python/ifrt/memory.h"
#include "xla/python/ifrt/shape.h"
#include "xla/python/ifrt/sharding.h"
#include "xla/python/ifrt/test_util.h"
#include "xla/status_macros.h"
#include "xla/tsl/concurrency/ref_count.h"
#include "tsl/platform/statusor.h"

namespace xla {
namespace ifrt {
namespace test_util {

IfrtIrExecutableImplTestBase::IfrtIrExecutableImplTestBase() {
  mlir::registerMLIRContextCLOptions();

  mlir::DialectRegistry registry;
  mlir::registerAllDialects(registry);
  mlir::mhlo::registerAllMhloDialects(registry);
  registry.insert<xla::ifrt::IfrtDialect>();
  xla::ifrt::AttachBuiltInSpmdExpansions(registry);
  mlir_context_.appendDialectRegistry(registry);
}

void IfrtIrExecutableImplTestBase::SetUp() {
  TF_ASSERT_OK_AND_ASSIGN(client_, GetClient());
}

absl::StatusOr<mlir::OwningOpRef<mlir::ModuleOp>>
IfrtIrExecutableImplTestBase::LoadFromSource(absl::string_view source) {
  mlir::BaseScopedDiagnosticHandler diagnostic_handler(&mlir_context_);
  auto op_ref = mlir::parseSourceString<mlir::ModuleOp>(source, &mlir_context_);
  if (!op_ref) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Failed to parse IFRT IR module string: %s",
                        diagnostic_handler.ConsumeStatus().message()));
  }
  return op_ref;
}

absl::StatusOr<mlir::OwningOpRef<mlir::ModuleOp>>
IfrtIrExecutableImplTestBase::LoadFromFile(absl::string_view file_path) {
  mlir::BaseScopedDiagnosticHandler diagnostic_handler(&mlir_context_);
  auto op_ref =
      mlir::parseSourceFile<mlir::ModuleOp>(file_path, &mlir_context_);
  if (!op_ref) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Failed to parse IFRT IR module file: %s",
                        diagnostic_handler.ConsumeStatus().message()));
  }
  return op_ref;
}

absl::StatusOr<tsl::RCReference<Array>>
IfrtIrExecutableImplTestBase::CreateArray(
    absl::Span<void* const> per_shard_data, Shape shape, DType dtype,
    ShardingParam sharding_param, tsl::RCReference<DeviceList> device_list) {
  TF_RET_CHECK(per_shard_data.size() == device_list->devices().size())
      << "Inconsistent sizes. per_shard_data " << per_shard_data.size()
      << " vs device_list " << device_list->devices().size();
  TF_ASSIGN_OR_RETURN(
      std::shared_ptr<const Sharding> sharding,
      ShardingParamSharding::Create(sharding_param, device_list, MemoryKind()));
  TF_ASSIGN_OR_RETURN(auto per_shard, sharding->Disassemble(shape));
  // All shards have the same shape. Just pick 0.
  Shape per_shard_shape = per_shard[0].first;
  std::vector<tsl::RCReference<Array>> per_shard_arrays;
  per_shard_arrays.reserve(per_shard_data.size());
  for (int i = 0; i < per_shard_data.size(); ++i) {
    TF_ASSIGN_OR_RETURN(
        tsl::RCReference<Array> per_shard_array,
        client_->MakeArrayFromHostBuffer(
            per_shard_data[i], dtype, per_shard_shape,
            /*byte_strides=*/std::nullopt,
            SingleDeviceSharding::Create(device_list->devices()[i],
                                         MemoryKind()),
            Client::HostBufferSemantics::kImmutableOnlyDuringCall,
            /*on_done_with_host_buffer=*/nullptr));
    per_shard_arrays.push_back(per_shard_array);
  }
  return client_->AssembleArrayFromSingleDeviceArrays(
      shape, sharding, absl::MakeSpan(per_shard_arrays),
      ArrayCopySemantics::kAlwaysCopy);
}

absl::StatusOr<tsl::RCReference<DeviceList>>
IfrtIrExecutableImplTestBase::PickDevices(int count) {
  absl::Span<Device* const> devices = client_->devices();
  TF_RET_CHECK(count <= devices.size())
      << "Requested " << count << " devices. Only have " << devices.size();
  auto picked = devices.first(count);
  return BasicDeviceList::Create(
      BasicDeviceList::Devices(picked.begin(), picked.end()));
}

}  // namespace test_util
}  // namespace ifrt
}  // namespace xla
