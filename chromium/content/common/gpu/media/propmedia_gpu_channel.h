// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_MEDIA_PROPMEDIA_GPU_CHANNEL_H_
#define CONTENT_COMMON_GPU_MEDIA_PROPMEDIA_GPU_CHANNEL_H_

#if defined(USE_SYSTEM_PROPRIETARY_CODECS)

#include "content/common/gpu/media/ipc_media_pipeline.h"
#include "gpu/ipc/service/gpu_channel.h"

namespace gpu {


class ProprietaryMediaGpuChannel : public GpuChannel
{
 public:
  // Takes ownership of the renderer process handle.
  ProprietaryMediaGpuChannel(GpuChannelManager* gpu_channel_manager,
             SyncPointManager* sync_point_manager,
             GpuWatchdogThread* watchdog,
             gl::GLShareGroup* share_group,
             gles2::MailboxManager* mailbox_manager,
             PreemptionFlag* preempting_flag,
             PreemptionFlag* preempted_flag,
             base::SingleThreadTaskRunner* task_runner,
             base::SingleThreadTaskRunner* io_task_runner,
             int32_t client_id,
             uint64_t client_tracing_id,
             bool allow_view_command_buffers,
             bool allow_real_time_streams);

   ~ProprietaryMediaGpuChannel() override;

 private:
  bool OnControlMessageReceived(const IPC::Message& msg) override;
  void OnNewMediaPipeline(int32_t route_id,
                          int32_t gpu_video_accelerator_factories_route_id);
  void OnDestroyMediaPipeline(int32_t route_id);

 private:
#if defined(USE_SYSTEM_PROPRIETARY_CODECS)
  typedef IDMap<content::IPCMediaPipeline, IDMapOwnPointer> MediaPipelineMap;
  MediaPipelineMap media_pipelines_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ProprietaryMediaGpuChannel);
};

}  // namespace gpu

#endif  // defined(USE_SYSTEM_PROPRIETARY_CODECS)


#endif  // CONTENT_COMMON_GPU_MEDIA_PROPMEDIA_GPU_CHANNEL_H_

