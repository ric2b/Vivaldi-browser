/* Copyright 2017 The OpenXLA Authors.

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

#include "xla/service/cpu/xfeed_manager.h"

#include "xla/shape_util.h"
#include "tsl/platform/logging.h"

namespace xla {
namespace cpu {
namespace runtime {

void XfeedQueueManager::EnqueueBuffersAtomically(
    absl::Span<XfeedBuffer* const> buffers) {
  absl::MutexLock l(&mu_);
  bool was_empty = enqueued_buffers_.empty();
  for (XfeedBuffer* b : buffers) {
    VLOG(3) << "Enqueueing " << queue_name_ << " buffer (of " << buffers.size()
            << " buffers) with length: " << b->length();
    enqueued_buffers_.push_back(b);
  }
  if (was_empty && !buffers.empty()) {
    // This has the potential to suffer from the notified thread
    // immediately trying and failing to acquire mu_, but seems
    // preferable to the alternative of notifying outside the lock
    // on every enqueue.
    cv_.Signal();
  }
}

XfeedBuffer* XfeedQueueManager::BlockingDequeueBuffer() {
  absl::MutexLock l(&mu_);
  VLOG(3) << "Waiting for an available buffer.";
  while (enqueued_buffers_.empty()) {
    cv_.Wait(&mu_);
  }
  VLOG(3) << "A buffer is available!";
  CHECK(current_buffer_ == nullptr);
  current_buffer_ = enqueued_buffers_.front();
  enqueued_buffers_.pop_front();
  return current_buffer_;
}

void XfeedQueueManager::ReleaseCurrentBuffer(int32_t length, void* data,
                                             absl::StatusOr<Shape> shape) {
  VLOG(3) << "Releasing buffer with shape: "
          << (shape.ok() ? ShapeUtil::HumanString(shape.value())
                         : "<error status>");
  absl::MutexLock l(&mu_);
  CHECK(current_buffer_ != nullptr);
  CHECK_EQ(length, current_buffer_->length());
  CHECK_EQ(data, current_buffer_->data());
  current_buffer_->Done(std::move(shape));
  current_buffer_ = nullptr;
}

int64_t GetByteSizeRequirement(const Shape& shape, int64_t pointer_size) {
  if (shape.IsTuple() || shape.is_static()) {
    return ShapeUtil::ByteSizeOf(shape, pointer_size);
  }
  int64_t metadata_size = sizeof(int32_t) * shape.dimensions_size();
  return ShapeUtil::ByteSizeOf(shape, pointer_size) + metadata_size;
}

}  // namespace runtime
}  // namespace cpu
}  // namespace xla
