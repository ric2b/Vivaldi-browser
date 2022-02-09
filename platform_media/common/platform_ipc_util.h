// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved.
//
// This file is an original work developed by Vivaldi Technologies AS.

#ifndef PLATFORM_MEDIA_COMMON_PLATFORM_IPC_UTIL_H_
#define PLATFORM_MEDIA_COMMON_PLATFORM_IPC_UTIL_H_

#include "platform_media/common/platform_media_pipeline_types.h"

namespace media {

// Minimal allocation size for shared memory region.
constexpr int kMinimalSharedMemorySize = 64 * 1024;

// Sanity limit on the shared memory size
constexpr int kMaxSharedMemorySize = 512 * 1024 * 1024;

// The size of the shared memory for the raw media buffer.
constexpr int kIPCSourceSharedMemorySize = 64 * 1024;

// Allocate at leasdt 4K as this is what is allocated in any case by the system.
constexpr size_t RoundUpTo4kPage(size_t size) {
  constexpr size_t page_mask = 4095;
  DCHECK(size < static_cast<size_t>(-1) - page_mask);
  return (size + page_mask) & ~page_mask;
}

}  // namespace media

#endif  // PLATFORM_MEDIA_COMMON_PLATFORM_IPC_UTIL_H_
