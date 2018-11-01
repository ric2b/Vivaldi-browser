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
                    GpuChannelManagerDelegate* delegate,
                    GpuWatchdogThread* watchdog,
                    base::SingleThreadTaskRunner* task_runner,
                    base::SingleThreadTaskRunner* io_task_runner,
                    base::WaitableEvent* shutdown_event,
                    SyncPointManager* sync_point_manager,
                    GpuMemoryBufferFactory* gpu_memory_buffer_factory,
                    const GpuFeatureInfo& gpu_feature_info)
   : GpuChannelManager(
                    gpu_preferences,
                    delegate,
                    watchdog,
                    task_runner,
                    io_task_runner,
                    shutdown_event,
                    sync_point_manager,
                    gpu_memory_buffer_factory,
                    gpu_feature_info) {}

ProprietaryMediaGpuChannelManager::~ProprietaryMediaGpuChannelManager() {}

std::unique_ptr<GpuChannel> ProprietaryMediaGpuChannelManager::CreateGpuChannel(
    int client_id,
    uint64_t client_tracing_id,
    bool preempts,
    bool allow_view_command_buffers,
    bool allow_real_time_streams) {
  return base::WrapUnique(
      new ProprietaryMediaGpuChannel(
                     this, sync_point_manager(), watchdog(), share_group(),
                     mailbox_manager(), preempts ? preemption_flag() : nullptr,
                     preempts ? nullptr : preemption_flag(), task_runner_.get(),
                     io_task_runner_.get(), client_id, client_tracing_id,
                     allow_view_command_buffers, allow_real_time_streams));
}

}  // namespace gpu

#endif // defined(USE_SYSTEM_PROPRIETARY_CODECS)
