// Copyright 2024 Google LLC.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef TENSORFLOW_LITE_EXPERIMENTAL_LITERT_CORE_AHWB_BUFFER_H_
#define TENSORFLOW_LITE_EXPERIMENTAL_LITERT_CORE_AHWB_BUFFER_H_

#include "tensorflow/lite/experimental/litert/c/litert_common.h"
#include "tensorflow/lite/experimental/litert/c/litert_event.h"

#if LITERT_HAS_AHWB_SUPPORT
#include <android/hardware_buffer.h>
#else
// Define a place holder AHardwareBuffer struct just to enable compilation.
#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus
typedef struct AHardwareBuffer AHardwareBuffer;
#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus
#endif  // LITERT_HAS_AHWB_SUPPORT

#include "absl/status/statusor.h"

namespace litert {
namespace internal {

struct AhwbBuffer {
  AHardwareBuffer* ahwb;

  static bool IsSupported();
  static absl::StatusOr<AhwbBuffer> Alloc(size_t size);
  static void Free(AHardwareBuffer* ahwb);
  static absl::StatusOr<size_t> GetSize(AHardwareBuffer* ahwb);
  static absl::StatusOr<void*> Lock(AHardwareBuffer* ahwb,
                                    LiteRtEvent event = nullptr);
  static absl::Status Unlock(AHardwareBuffer* ahwb);
};

}  // namespace internal
}  // namespace litert

#endif  // TENSORFLOW_LITE_EXPERIMENTAL_LITERT_CORE_AHWB_BUFFER_H_
