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

#include "xla/stream_executor/gpu/gpu_stream.h"

#include <cstdint>
#include <variant>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "xla/stream_executor/device_memory.h"
#include "xla/stream_executor/event.h"
#include "xla/stream_executor/gpu/gpu_driver.h"
#include "xla/stream_executor/gpu/gpu_event.h"
#include "xla/stream_executor/gpu/gpu_executor.h"
#include "xla/stream_executor/gpu/gpu_types.h"
#include "xla/stream_executor/platform.h"
#include "xla/stream_executor/stream.h"
#include "tsl/platform/errors.h"
#include "tsl/profiler/lib/nvtx_utils.h"

namespace stream_executor {
namespace gpu {

bool GpuStream::Init() {
  int priority = [&]() {
    if (std::holds_alternative<int>(stream_priority_)) {
      return std::get<int>(stream_priority_);
    }
    return GpuDriver::GetGpuStreamPriority(
        parent_->gpu_context(), std::get<StreamPriority>(stream_priority_));
  }();
  if (!GpuDriver::CreateStream(parent_->gpu_context(), &gpu_stream_,
                               priority)) {
    return false;
  }
  return GpuDriver::InitEvent(parent_->gpu_context(), &completed_event_,
                              GpuDriver::EventFlags::kDisableTiming)
      .ok();
}

Stream::PlatformSpecificHandle GpuStream::platform_specific_handle() const {
  PlatformSpecificHandle handle;
  handle.stream = gpu_stream_;
  return handle;
}

absl::Status GpuStream::Memset32(DeviceMemoryBase* location, uint32_t pattern,
                                 uint64_t size) {
  CHECK(reinterpret_cast<uintptr_t>(location->opaque()) % 4 == 0 &&
        size % 4 == 0);
  return GpuDriver::AsynchronousMemsetUint32(
      parent_->gpu_context(),
      reinterpret_cast<GpuDevicePtr>(location->opaque()), pattern, size / 4,
      gpu_stream());
}

absl::Status GpuStream::MemZero(DeviceMemoryBase* location, uint64_t size) {
  if (reinterpret_cast<uintptr_t>(location->opaque()) % 4 == 0 &&
      size % 4 == 0) {
    return Memset32(location, 0x0, size);
  } else {
    return parent_->Memset(this, location, 0x0, size);
  }
}

absl::Status GpuStream::Memcpy(DeviceMemoryBase* gpu_dst,
                               const DeviceMemoryBase& gpu_src, uint64_t size) {
  if (GpuDriver::AsynchronousMemcpyD2D(
          parent_->gpu_context(),
          reinterpret_cast<GpuDevicePtr>(const_cast<void*>(gpu_dst->opaque())),
          reinterpret_cast<GpuDevicePtr>(const_cast<void*>(gpu_src.opaque())),
          size, gpu_stream())) {
    return absl::OkStatus();
  }

  return absl::InternalError("Failed to memcpy from device to device.");
}

absl::Status GpuStream::Memcpy(DeviceMemoryBase* gpu_dst, const void* host_src,
                               uint64_t size) {
  bool ok = GpuDriver::AsynchronousMemcpyH2D(
      parent_->gpu_context(), reinterpret_cast<GpuDevicePtr>(gpu_dst->opaque()),
      host_src, size, gpu_stream());
  if (!ok) {
    return absl::InternalError("Failed to memcpy from device to host.");
  }
  return absl::OkStatus();
}

absl::Status GpuStream::Memcpy(void* host_dst, const DeviceMemoryBase& gpu_src,
                               uint64_t size) {
  bool ok = GpuDriver::AsynchronousMemcpyD2H(
      parent_->gpu_context(), host_dst,
      reinterpret_cast<GpuDevicePtr>(const_cast<void*>(gpu_src.opaque())), size,
      gpu_stream());
  // TODO(b/326130105): Change AsynchronousMemcpyD2H calls to return
  // absl::Status.
  if (!ok) {
    return absl::InternalError("Failed to memcpy from device to host.");
  }
  return absl::OkStatus();
}

absl::Status GpuStream::WaitFor(Stream* other) {
  GpuStream* other_gpu = AsGpuStream(other);
  GpuEventHandle other_completed_event = *(other_gpu->completed_event());
  TF_RETURN_IF_ERROR(GpuDriver::RecordEvent(parent_->gpu_context(),
                                            other_completed_event,
                                            AsGpuStreamValue(other_gpu)));

  if (GpuDriver::WaitStreamOnEvent(parent_->gpu_context(),
                                   AsGpuStreamValue(this),
                                   other_completed_event)) {
    return absl::OkStatus();
  }
  return absl::InternalError("Couldn't wait for stream.");
}

absl::Status GpuStream::RecordEvent(Event* event) {
  return static_cast<GpuEvent*>(event)->Record(gpu_stream_);
}

absl::Status GpuStream::WaitFor(Event* event) {
  if (GpuDriver::WaitStreamOnEvent(
          parent_->gpu_context(), gpu_stream(),
          static_cast<GpuEvent*>(event)->gpu_event())) {
    return absl::OkStatus();
  } else {
    return absl::InternalError(absl::StrFormat(
        "error recording waiting for event on stream %p", this));
  }
}

void GpuStream::Destroy() {
  if (completed_event_ != nullptr) {
    absl::Status status =
        GpuDriver::DestroyEvent(parent_->gpu_context(), &completed_event_);
    if (!status.ok()) {
      LOG(ERROR) << status.message();
    }
  }

  GpuDriver::DestroyStream(parent_->gpu_context(), &gpu_stream_);
}

bool GpuStream::IsIdle() const {
  return GpuDriver::IsStreamIdle(parent_->gpu_context(), gpu_stream_);
}

void GpuStream::set_name(absl::string_view name) {
  name_ = name;
  tsl::profiler::NameStream(
      reinterpret_cast<tsl::profiler::StreamHandle>(gpu_stream()), name_);
}

GpuStream* AsGpuStream(Stream* stream) {
  DCHECK(stream != nullptr);
  return static_cast<GpuStream*>(stream);
}

GpuStreamHandle AsGpuStreamValue(Stream* stream) {
  DCHECK(stream != nullptr);
  return AsGpuStream(stream)->gpu_stream();
}

}  // namespace gpu
}  // namespace stream_executor
