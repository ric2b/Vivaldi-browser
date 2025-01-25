/* Copyright 2018 The OpenXLA Authors.

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

#ifndef XLA_STREAM_EXECUTOR_ROCM_ROCM_PLATFORM_H_
#define XLA_STREAM_EXECUTOR_ROCM_ROCM_PLATFORM_H_

#include <memory>
#include <vector>

#include "absl/synchronization/mutex.h"
#include "xla/stream_executor/executor_cache.h"
#include "xla/stream_executor/platform.h"
#include "xla/stream_executor/platform/port.h"
#include "xla/stream_executor/platform_manager.h"
#include "xla/stream_executor/stream_executor.h"

namespace stream_executor {
namespace gpu {

// Opaque and unique identifier for the ROCM platform plugin.
// This is needed so that plugins can refer to/identify this platform without
// instantiating a ROCmPlatform object.
extern const Platform::Id kROCmPlatformId;

// ROCm-specific platform plugin, registered as a singleton value via module
// initializer.
class ROCmPlatform : public Platform {
 public:
  ROCmPlatform();
  ~ROCmPlatform() override;

  // ROCmPlatform-specific functionality
  // Returns the number of distinct buses / NUMA nodes on the machine.
  int BusCount();

  // Returns the bus/NUMA node for the specified device ordinal.
  int DeviceToBus(int device_ordinal);

  // Returns the lowest-ordinal-number StreamExecutor on the specified bus.
  absl::StatusOr<StreamExecutor*> FirstExecutorForBus(int bus_ordinal);

  // Platform interface implementation:
  // Returns the same value as kROCmPlatform above.
  Platform::Id id() const override;

  // Returns -1 as a sentinel on internal failure (and logs the error).
  int VisibleDeviceCount() const override;

  const std::string& Name() const override;

  absl::StatusOr<std::unique_ptr<DeviceDescription>> DescriptionForDevice(
      int ordinal) const override;

  absl::StatusOr<StreamExecutor*> ExecutorForDevice(int ordinal) override;

  absl::StatusOr<StreamExecutor*> GetExecutor(
      const StreamExecutorConfig& config) override;

  absl::StatusOr<std::unique_ptr<StreamExecutor>> GetUncachedExecutor(
      const StreamExecutorConfig& config) override;

 private:
  // Determines the number of NUMA nodes and the assignment of executor to each.
  void InspectNumaNodes();

  // This platform's name.
  std::string name_;

  // mutex that guards internal state.
  mutable absl::Mutex mu_;

  // Cache of created executors.
  ExecutorCache executor_cache_;

  // The smallest NUMA node value for any device managed by this machine
  // manager. Used, along with limit_numa_node_, to convert NUMA nodes into bus
  // ordinals. The NUMA node space occupied by GPUs is assumed to be dense./
  int min_numa_node_;

  // Larger than the NUMA node value for any device managed by this machine
  // manager.
  int limit_numa_node_;

  ROCmPlatform(const ROCmPlatform&) = delete;
  void operator=(const ROCmPlatform&) = delete;
};

}  // namespace gpu
}  // namespace stream_executor

#endif  // XLA_STREAM_EXECUTOR_ROCM_ROCM_PLATFORM_H_
