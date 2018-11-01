// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform_media/gpu/pipeline/propmedia_gpu_channel_manager.h"

#if defined(USE_SYSTEM_PROPRIETARY_CODECS)

#include "base/memory/ptr_util.h"
#include "platform_media/gpu/pipeline/propmedia_gpu_channel.h"

namespace gpu {

ProprietaryMediaGpuChannelManager::ProprietaryMediaGpuChannelManager(
                    const GpuPreferences& gpu_preferences,
                    const GpuDriverBugWorkarounds& workarounds,
                    GpuChannelManagerDelegate* delegate,
                    GpuWatchdogThread* watchdog,
                    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
                    SyncPointManager* sync_point_manager,
                    GpuMemoryBufferFactory* gpu_memory_buffer_factory,
                    const GpuFeatureInfo& gpu_feature_info,
                    GpuProcessActivityFlags activity_flags)
   : GpuChannelManager(
                    gpu_preferences,
                    workarounds,
                    delegate,
                    watchdog,
                    task_runner,
                    io_task_runner,
                    sync_point_manager,
                    gpu_memory_buffer_factory,
                    gpu_feature_info,
                    std::move(activity_flags)) {}

ProprietaryMediaGpuChannelManager::~ProprietaryMediaGpuChannelManager() {}

GpuChannel* ProprietaryMediaGpuChannelManager::EstablishChannel(
    int client_id,
    uint64_t client_tracing_id,
    bool is_gpu_host) {

    std::unique_ptr<ProprietaryMediaGpuChannel> gpu_channel = base::MakeUnique<ProprietaryMediaGpuChannel>(
                this, sync_point_manager(), watchdog(), share_group(),
                mailbox_manager(), is_gpu_host ? preemption_flag() : nullptr,
                is_gpu_host ? nullptr : preemption_flag(), task_runner_.get(),
                io_task_runner_.get(), client_id, client_tracing_id,
                is_gpu_host);
    GpuChannel* gpu_channel_ptr = gpu_channel.get();
    gpu_channels_[client_id] = std::move(gpu_channel);
    return gpu_channel_ptr;
}

}  // namespace gpu

#endif // defined(USE_SYSTEM_PROPRIETARY_CODECS)
