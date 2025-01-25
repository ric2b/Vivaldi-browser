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

#include "xla/service/cpu/runtime/logical_id_thunk.h"

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "xla/executable_run_options.h"
#include "xla/service/buffer_assignment.h"
#include "xla/service/cpu/runtime/buffer_allocations.h"
#include "xla/service/cpu/runtime/thunk.h"
#include "xla/service/maybe_owning_device_memory.h"
#include "xla/stream_executor/device_memory.h"
#include "xla/tsl/concurrency/async_value_ref.h"
#include "tsl/platform/statusor.h"
#include "tsl/platform/test.h"

namespace xla::cpu {
namespace {

absl::StatusOr<DeviceAssignment> CreateDeviceAssignment(
    std::vector<std::vector<int64_t>> devices) {
  const auto computation_count = devices.size();
  if (devices.empty()) {
    return absl::InternalError("Devices must not be empty.");
  }
  const auto replica_count = devices[0].size();
  DeviceAssignment device_assignment(replica_count, computation_count);
  for (int64_t partition = 0; partition < computation_count; ++partition) {
    for (int64_t replica = 0; replica < replica_count; ++replica) {
      device_assignment(replica, partition) = devices[partition][replica];
    }
  }
  return device_assignment;
}

TEST(LogicalIdThunkTest, GetReplicaId) {
  std::vector<int32_t> dst(1, std::numeric_limits<int32_t>::min());

  std::vector<MaybeOwningDeviceMemory> buffers;
  buffers.emplace_back(se::DeviceMemoryBase(dst.data(), sizeof(int32_t)));

  BufferAllocation alloc(/*index=*/0, /*size=*/sizeof(int32_t), /*color=*/0);
  BufferAllocation::Slice id_slice(&alloc, /*offset=*/0,
                                   /*size=*/sizeof(int32_t));

  std::string name(Thunk::KindToString(Thunk::Kind::kReplicaId));
  TF_ASSERT_OK_AND_ASSIGN(auto thunk, ReplicaIdThunk::Create({name}, id_slice));

  BufferAllocations allocations(buffers);
  TF_ASSERT_OK_AND_ASSIGN(DeviceAssignment device_assn,
                          CreateDeviceAssignment({{0, 1}}));

  ExecutableRunOptions run_options;
  run_options.set_device_ordinal(0);
  run_options.set_device_assignment(&device_assn);

  TF_ASSERT_OK_AND_ASSIGN(Thunk::CollectiveExecuteParams collective_params,
                          Thunk::CollectiveExecuteParams::Create(&run_options));

  Thunk::ExecuteParams params;
  params.buffer_allocations = &allocations;
  params.collective_params = &collective_params;

  auto execute_event = thunk->Execute(params);
  tsl::BlockUntilReady(execute_event);
  ASSERT_FALSE(execute_event.IsError());

  EXPECT_EQ(dst[0], 0);
}

TEST(LogicalIdThunkTest, GetPartitionId) {
  std::vector<int32_t> dst(2, std::numeric_limits<int32_t>::min());

  std::vector<MaybeOwningDeviceMemory> buffers;
  static constexpr auto kDataSize = 2 * sizeof(int32_t);
  buffers.emplace_back(se::DeviceMemoryBase(dst.data(), kDataSize));

  BufferAllocation alloc(/*index=*/0, /*size=*/kDataSize, /*color=*/0);
  BufferAllocation::Slice id_slice(&alloc, /*offset=*/sizeof(int32_t),
                                   /*size=*/sizeof(int32_t));

  std::string name(Thunk::KindToString(Thunk::Kind::kPartitionId));
  TF_ASSERT_OK_AND_ASSIGN(auto thunk,
                          PartitionIdThunk::Create({name}, id_slice));

  BufferAllocations allocations(buffers);
  TF_ASSERT_OK_AND_ASSIGN(DeviceAssignment device_assn,
                          CreateDeviceAssignment({{0}, {1}}));

  ExecutableRunOptions run_options;
  run_options.set_device_ordinal(0);
  run_options.set_device_assignment(&device_assn);

  TF_ASSERT_OK_AND_ASSIGN(Thunk::CollectiveExecuteParams collective_params,
                          Thunk::CollectiveExecuteParams::Create(&run_options));

  Thunk::ExecuteParams params;
  params.buffer_allocations = &allocations;
  params.collective_params = &collective_params;

  auto execute_event = thunk->Execute(params);
  tsl::BlockUntilReady(execute_event);
  ASSERT_FALSE(execute_event.IsError());

  EXPECT_EQ(dst[0], std::numeric_limits<int32_t>::min());
  EXPECT_EQ(dst[1], 0);
}

}  // namespace
}  // namespace xla::cpu
