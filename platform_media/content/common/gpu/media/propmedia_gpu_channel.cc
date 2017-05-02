// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/media/propmedia_gpu_channel.h"

#if defined(USE_SYSTEM_PROPRIETARY_CODECS)

#include "base/command_line.h"
#include "build/build_config.h"

#include "content/common/gpu/media/ipc_media_pipeline.h"
#include "content/common/media/media_pipeline_messages.h"
#include "content/public/common/content_switches.h"
#include "gpu/config/gpu_switches.h"
#include "gpu/ipc/common/gpu_messages.h"
#include "ipc/ipc_channel.h"
#include "ipc/message_filter.h"

namespace gpu {

ProprietaryMediaGpuChannel::ProprietaryMediaGpuChannel(
             GpuChannelManager* gpu_channel_manager,
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
             bool allow_real_time_streams)
  : GpuChannel(
             gpu_channel_manager,
             sync_point_manager,
             watchdog,
             share_group,
             mailbox_manager,
             preempting_flag,
             preempted_flag,
             task_runner,
             io_task_runner,
             client_id,
             client_tracing_id,
             allow_view_command_buffers,
             allow_real_time_streams) {}

ProprietaryMediaGpuChannel::~ProprietaryMediaGpuChannel() {}

bool ProprietaryMediaGpuChannel::OnControlMessageReceived(
      const IPC::Message& msg
    ) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ProprietaryMediaGpuChannel, msg)
#if defined(USE_SYSTEM_PROPRIETARY_CODECS)
    IPC_MESSAGE_HANDLER(MediaPipelineMsg_New, OnNewMediaPipeline)
    IPC_MESSAGE_HANDLER(MediaPipelineMsg_Destroy, OnDestroyMediaPipeline)
#endif
    IPC_MESSAGE_UNHANDLED(handled = GpuChannel::OnControlMessageReceived(msg))
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ProprietaryMediaGpuChannel::OnNewMediaPipeline(
    int32_t route_id,
    int32_t gpu_video_accelerator_factories_route_id) {
  GpuCommandBufferStub* command_buffer = nullptr;

  const base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (!cmd_line->HasSwitch(switches::kDisableAcceleratedVideoDecode) &&
      cmd_line->HasSwitch(switches::kEnablePlatformAcceleratedVideoDecoding)) {
    command_buffer =
        LookupCommandBuffer(gpu_video_accelerator_factories_route_id);
  }

  scoped_refptr<GpuChannelMessageQueue> queue = LookupStream(
        gpu_video_accelerator_factories_route_id);
  if (!queue)
    queue = CreateStream(gpu_video_accelerator_factories_route_id,
                         GpuStreamPriority::HIGH);

  std::unique_ptr<content::IPCMediaPipeline> ipc_media_pipeline(
      new content::IPCMediaPipeline(this, route_id, command_buffer));
  AddRoute(route_id, gpu_video_accelerator_factories_route_id,
           ipc_media_pipeline.get());
  media_pipelines_.AddWithID(std::move(ipc_media_pipeline), route_id);
}

void ProprietaryMediaGpuChannel::OnDestroyMediaPipeline(int32_t route_id) {
  RemoveRoute(route_id);
  media_pipelines_.Remove(route_id);
}

}  // namespace gpu

#endif
