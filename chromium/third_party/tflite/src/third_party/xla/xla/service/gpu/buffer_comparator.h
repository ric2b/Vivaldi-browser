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

#ifndef XLA_SERVICE_GPU_BUFFER_COMPARATOR_H_
#define XLA_SERVICE_GPU_BUFFER_COMPARATOR_H_

#include "absl/status/statusor.h"
#include "xla/service/hlo_module_config.h"
#include "xla/shape.h"
#include "xla/stream_executor/device_memory.h"
#include "xla/stream_executor/stream_executor.h"

#if TENSORFLOW_USE_ROCM
#include "rocm/rocm_config.h"
#endif

namespace xla::gpu {

// A device-side comparator that compares buffers.
class BufferComparator {
 public:
  BufferComparator(const BufferComparator&) = delete;
  BufferComparator(BufferComparator&&) = default;

  explicit BufferComparator(const Shape& shape, double tolerance = 0.1,
                            bool verbose = true);

  // Returns true if the two buffers compare equal. The definition of "equal"
  // is:
  // * All NaNs equal.
  // * All fp16 infs are treated as 65505 or -65505. Otherwise,
  //   infs and negative infs compare equal.
  // * With NaNs and infs taken care of, a and b compare equal iff:
  //     abs(a - b) / (max(abs(a), abs(b)) + 1) < tolerance
  //
  // See the implementation for the tolerance value.
  absl::StatusOr<bool> CompareEqual(se::Stream* stream,
                                    se::DeviceMemoryBase current,
                                    se::DeviceMemoryBase expected) const;
 private:
  Shape shape_;
  double relative_tol_;  // relative tolerance for comparison
  bool verbose_;         // whether to print out error message on mismatch
};

namespace buffer_comparator {

// Returns a pointer to CUDA C++ device function implementing comparison.
void* fp8_e4m3fn_comparison();
void* fp8_e5m2_comparison();
#if TENSORFLOW_USE_ROCM && TF_ROCM_VERSION >= 60200
void* fp8_e4m3fnuz_comparison();
void* fp8_e5m2fnuz_comparison();
#endif  // TENSORFLOW_USE_ROCM && TF_ROCM_VERSION >= 60200
void* fp16_comparison();
void* bf16_comparison();
void* fp32_comparison();
void* fp64_comparison();
void* int8_comparison();
void* int32_comparison();

}  // namespace buffer_comparator
}  // namespace xla::gpu

#endif  // XLA_SERVICE_GPU_BUFFER_COMPARATOR_H_
