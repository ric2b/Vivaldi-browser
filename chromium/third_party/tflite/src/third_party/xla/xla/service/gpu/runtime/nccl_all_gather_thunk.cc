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

#include "xla/service/gpu/runtime/nccl_all_gather_thunk.h"

#include <cstdint>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "xla/hlo/ir/hlo_instruction.h"
#include "xla/hlo/ir/hlo_instructions.h"
#include "xla/service/collective_ops_utils.h"
#include "xla/service/gpu/backend_configs.pb.h"
#include "xla/service/gpu/runtime/nccl_api.h"
#include "xla/service/gpu/runtime/nccl_collective_thunk.h"
#include "xla/service/gpu/runtime/thunk.h"
#include "xla/shape.h"
#include "xla/shape_util.h"
#include "xla/stream_executor/stream.h"
#include "tsl/platform/errors.h"
#include "tsl/platform/logging.h"
#include "tsl/platform/statusor.h"

namespace xla {
namespace gpu {

namespace impl {
NcclAllGatherConfig GetNcclAllGatherConfig(
    const HloAllGatherInstruction* inst) {
  NcclAllGatherConfig config;
  config.config = GetNcclCollectiveConfig(inst, inst->use_global_device_ids());
  return config;
}

absl::Status CheckImplementableInst(const HloAllGatherInstruction* inst) {
  for (HloInstruction* operand : inst->operands()) {
    const Shape& shape = operand->shape();

    TF_RETURN_IF_ERROR(IsValidOperand(shape, Thunk::kNcclAllGather));

    if (!ShapeUtil::IsEffectivelyMostMajorDimension(
            shape, inst->all_gather_dimension())) {
      return absl::AbortedError(absl::StrFormat(
          "all-gather dim %u is not the most major in input shape %s",
          inst->all_gather_dimension(), shape.ToString(/*print_layout=*/true)));
    }
  }

  return absl::OkStatus();
}
}  // namespace impl

NcclAllGatherStartThunk::NcclAllGatherStartThunk(
    ThunkInfo thunk_info, NcclApi* nccl_api,
    const HloAllGatherInstruction* inst, std::vector<Buffer> buffers)
    : NcclCollectiveThunk(Thunk::kNcclAllGatherStart, thunk_info, nccl_api,
                          IsSyncCollective(inst)),
      config_(impl::GetNcclAllGatherConfig(inst)),
      buffers_(std::move(buffers)) {
  CHECK_EQ(config_.config.operand_count, buffers_.size());
}

/*static*/ absl::Status NcclAllGatherStartThunk::CheckImplementable(
    const HloAllGatherInstruction* inst, int64_t replica_count,
    int64_t partition_count) {
  return AddOpDescription<NcclAllGatherStartThunk>(
      impl::CheckImplementableInst(inst), inst, replica_count, partition_count);
}

/*static*/ CollectiveOpGroupMode NcclAllGatherStartThunk::GetGroupMode(
    const HloAllGatherInstruction* inst) {
  return impl::GetNcclAllGatherConfig(inst).config.group_mode;
}

absl::Status NcclAllGatherStartThunk::RunNcclCollective(
    const ExecuteParams& params, se::Stream& stream,
    NcclCommHandleWrapper comm_wrapper) {
  TF_ASSIGN_OR_RETURN(
      std::vector<DeviceBufferPair> device_buffers,
      ConvertToDeviceBuffers(params, buffers_,
                             config_.config.operand_element_type));
  return xla::gpu::RunAllGather(nccl_api(), device_buffers, stream,
                                comm_wrapper.comm_handle);
}

absl::Status RunAllGather(NcclApi* nccl_api,
                          std::vector<DeviceBufferPair>& buffers,
                          se::Stream& stream, NcclApi::NcclCommHandle comm) {
  int device_ordinal = stream.parent()->device_ordinal();
  VLOG(3) << "Performing all-gather from device ordinal: " << device_ordinal;
  TF_RETURN_IF_ERROR(
      MaybeRegisterBuffers(nccl_api, device_ordinal, buffers, comm));

  TF_RETURN_IF_ERROR(nccl_api->GroupStart());

  for (DeviceBufferPair& buffer : buffers) {
    TF_RETURN_IF_ERROR(nccl_api->AllGather(
        buffer.source_buffer, buffer.destination_buffer, buffer.element_type,
        buffer.element_count, comm, &stream));
  }

  TF_RETURN_IF_ERROR(nccl_api->GroupEnd());

  VLOG(3) << "Done performing all-gather for ordinal: " << device_ordinal;
  return absl::OkStatus();
}

}  // namespace gpu
}  // namespace xla
