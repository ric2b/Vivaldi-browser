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
                    const GpuDriverBugWorkarounds& workarounds,
                    GpuChannelManagerDelegate* delegate,
                    GpuWatchdogThread* watchdog,
                    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
                    SyncPointManager* sync_point_manager,
                    GpuMemoryBufferFactory* gpu_memory_buffer_factory,
                    const GpuFeatureInfo& gpu_feature_info,
                    GpuProcessActivityFlags activity_flags);

  ~ProprietaryMediaGpuChannelManager() override;

 protected:
  GpuChannel* EstablishChannel(
      int client_id,
      uint64_t client_tracing_id,
      bool is_gpu_host) override;

  DISALLOW_COPY_AND_ASSIGN(ProprietaryMediaGpuChannelManager);
};

}  // namespace gpu
#endif  // defined(USE_SYSTEM_PROPRIETARY_CODECS)

#endif  // CONTENT_COMMON_GPU_MEDIA_PROPMEDIA_GPU_CHANNEL_MANAGER_H_
