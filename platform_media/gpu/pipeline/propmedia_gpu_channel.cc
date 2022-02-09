// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform_media/gpu/pipeline/propmedia_gpu_channel.h"

#include "gpu/command_buffer/common/scheduling_priority.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/ipc/service/gpu_channel.h"

#include "platform_media/gpu/pipeline/ipc_media_pipeline.h"

namespace gpu {

std::unique_ptr<PropmediaGpuChannel::PipelineBase> (
    *PropmediaGpuChannel::g_create_pipeline)() = nullptr;

PropmediaGpuChannel::PropmediaGpuChannel() = default;
PropmediaGpuChannel::~PropmediaGpuChannel() = default;

/* static */
void PropmediaGpuChannel::StartNewMediaPipeline(
    base::WeakPtr<GpuChannel> channel,
    gpu::mojom::VivaldiMediaPipelineParamsPtr params) {
  if (!channel)
    return;
  PropmediaGpuChannel& self = channel->prop_media_gpu_channel;
  std::unique_ptr<PipelineBase> ipc_media_pipeline = g_create_pipeline();

  // Initialize the channel route before any IPC that the pipeline
  // initialization may trigger. so we can receive IPC messages to ensure that we can receive IPC messages
  // inside pipeline->Initialize() below.
  PipelineBase* pipeline = ipc_media_pipeline.get();
  SequenceId sequence_id;
  if (channel->scheduler()) {
    sequence_id =
        channel->scheduler()->CreateSequence(SchedulingPriority::kNormal);
  }
  channel->AddRoute(params->route_id, sequence_id, pipeline);
  self.pipelines_[params->route_id] = std::move(ipc_media_pipeline);

  pipeline->Initialize(channel.get(), std::move(params));
}

/* static */
void PropmediaGpuChannel::DestroyMediaPipeline(
    base::WeakPtr<GpuChannel> channel,
    int32_t route_id) {
  if (!channel)
    return;
  PropmediaGpuChannel& self = channel->prop_media_gpu_channel;
  channel->RemoveRoute(route_id);
  self.pipelines_.erase(route_id);
}

}  // namespace gpu
