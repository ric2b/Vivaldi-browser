// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform_media/gpu/pipeline/propmedia_gpu_channel.h"

#include "platform_media/common/media_pipeline_messages.h"
#include "platform_media/gpu/pipeline/ipc_media_pipeline.h"

#include "base/command_line.h"
#include "gpu/command_buffer/common/scheduling_priority.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/config/gpu_switches.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "gpu/ipc/service/gpu_channel_manager.h"

namespace gpu {

ProprietaryMediaGpuChannel::ProprietaryMediaGpuChannel(gpu::GpuChannel* channel)
    : channel_(channel) {}

ProprietaryMediaGpuChannel::~ProprietaryMediaGpuChannel() {}

bool ProprietaryMediaGpuChannel::Send(IPC::Message* msg) {
  return channel_->Send(msg);
}

bool ProprietaryMediaGpuChannel::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ProprietaryMediaGpuChannel, msg)
    IPC_MESSAGE_HANDLER(MediaPipelineMsg_New, OnNewMediaPipeline)
    IPC_MESSAGE_HANDLER(MediaPipelineMsg_Destroy, OnDestroyMediaPipeline)
    IPC_MESSAGE_UNHANDLED(handled = OnPipelineMessageReceived(msg))
  IPC_END_MESSAGE_MAP()
  return handled;
}

bool ProprietaryMediaGpuChannel::OnPipelineMessageReceived(
    const IPC::Message& msg) {

  if (channel_->scheduler())
    return false;

  media::IPCMediaPipeline* pipeline = media_pipelines_.Lookup(msg.routing_id());

  if (pipeline)
      return pipeline->OnMessageReceived(msg);

  return false;
}

void ProprietaryMediaGpuChannel::OnNewMediaPipeline(
    int32_t route_id,
    int32_t command_buffer_route_id) {

  std::unique_ptr<media::IPCMediaPipeline> ipc_media_pipeline(
      new media::IPCMediaPipeline(this, route_id));

  if (channel_->scheduler()) {
    SequenceId sequence_id = channel_->scheduler()->CreateSequence(SchedulingPriority::kNormal);
    channel_->AddRoute(route_id, sequence_id, ipc_media_pipeline.get());
  }

  media_pipelines_.AddWithID(std::move(ipc_media_pipeline), route_id);
}

void ProprietaryMediaGpuChannel::OnDestroyMediaPipeline(int32_t route_id) {
  media_pipelines_.Remove(route_id);

  if (channel_->scheduler()) {
    channel_->RemoveRoute(route_id);
  }
}
}  // namespace gpu
