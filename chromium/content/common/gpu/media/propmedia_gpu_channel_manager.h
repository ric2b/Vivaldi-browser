// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_MEDIA_PROPMEDIA_GPU_CHANNEL_MANAGER_H_
#define CONTENT_COMMON_GPU_MEDIA_PROPMEDIA_GPU_CHANNEL_MANAGER_H_

#if defined(USE_SYSTEM_PROPRIETARY_CODECS)

#include "gpu/ipc/service/gpu_channel_manager.h"

namespace gpu {

class ProprietaryMediaGpuChannelManager : public GpuChannelManager {
 public:
  ProprietaryMediaGpuChannelManager(const GpuPreferences& gpu_preferences,
                    GpuChannelManagerDelegate* delegate,
                    GpuWatchdog* watchdog,
                    base::SingleThreadTaskRunner* task_runner,
                    base::SingleThreadTaskRunner* io_task_runner,
                    base::WaitableEvent* shutdown_event,
                    SyncPointManager* sync_point_manager,
                    GpuMemoryBufferFactory* gpu_memory_buffer_factory);

  ~ProprietaryMediaGpuChannelManager() override;

 protected:
  std::unique_ptr<GpuChannel> CreateGpuChannel(
      int client_id,
      uint64_t client_tracing_id,
      bool preempts,
      bool allow_view_command_buffers,
      bool allow_real_time_streams) override;

  DISALLOW_COPY_AND_ASSIGN(ProprietaryMediaGpuChannelManager);
};

}  // namespace gpu
#endif  // defined(USE_SYSTEM_PROPRIETARY_CODECS)

#endif  // CONTENT_COMMON_GPU_MEDIA_PROPMEDIA_GPU_CHANNEL_MANAGER_H_
