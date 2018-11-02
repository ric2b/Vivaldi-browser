// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_MEDIA_GPU_PIPELINE_PROPMEDIA_GPU_CHANNEL_H_
#define PLATFORM_MEDIA_GPU_PIPELINE_PROPMEDIA_GPU_CHANNEL_H_

#include "base/containers/id_map.h"

#include "ipc/ipc_listener.h"
#include "ipc/ipc_sender.h"

namespace content {
class IPCMediaPipeline;
}

namespace gpu {

class GpuChannel;

class ProprietaryMediaGpuChannel : public IPC::Listener, public IPC::Sender
{
 public:

  ProprietaryMediaGpuChannel(gpu::GpuChannel* channel);
  ~ProprietaryMediaGpuChannel() override;

  // IPC::Sender implementation:
  bool Send(IPC::Message* msg) override;

  // IPC::Listener implementation:
  bool OnMessageReceived(const IPC::Message& msg) override;

 private:

  void OnNewMediaPipeline(int32_t route_id, int32_t command_buffer_route_id);
  void OnDestroyMediaPipeline(int32_t route_id);
  bool OnPipelineMessageReceived(const IPC::Message& message);
  bool AllowHardwareDecode();

  gpu::GpuChannel* const channel_;
  base::IDMap<std::unique_ptr<content::IPCMediaPipeline>> media_pipelines_;

  DISALLOW_COPY_AND_ASSIGN(ProprietaryMediaGpuChannel);
};

}  // namespace gpu

#endif  // PLATFORM_MEDIA_GPU_PIPELINE_PROPMEDIA_GPU_CHANNEL_H_
