// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_MEDIA_GPU_PIPELINE_PROPMEDIA_GPU_CHANNEL_H_
#define PLATFORM_MEDIA_GPU_PIPELINE_PROPMEDIA_GPU_CHANNEL_H_

#include "platform_media/common/feature_toggles.h"

#include <map>

#include "base/memory/weak_ptr.h"
#include "gpu/ipc/common/gpu_channel.mojom.h"
#include "gpu/ipc/service/gpu_ipc_service_export.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_sender.h"

namespace gpu {

class GpuChannel;

// Helpers extending GpuChannel functionality to support IPC pipeline.
class PropmediaGpuChannel {
 public:
  // Due to linking dependency limitations we cannot reference IPCMediaPipeline
  // from this code. So we use an indirection via this class and a pointer to
  // a function that creates its instance. The pointer is initialized in
  // the GpuServiceImpl constructor.
  class PipelineBase : public IPC::Listener {
   public:
    ~PipelineBase() override = default;
    virtual void Initialize(
        IPC::Sender* channel,
        gpu::mojom::VivaldiMediaPipelineParamsPtr params) = 0;
  };

  static GPU_IPC_SERVICE_EXPORT std::unique_ptr<PipelineBase> (
      *g_create_pipeline)();

  PropmediaGpuChannel();
  ~PropmediaGpuChannel();

  static void StartNewMediaPipeline(
      base::WeakPtr<GpuChannel> gpu_channel,
      gpu::mojom::VivaldiMediaPipelineParamsPtr params);

  static void DestroyMediaPipeline(base::WeakPtr<GpuChannel> gpu_channel,
                                   int32_t route_id);

 private:
  std::map<int32_t, std::unique_ptr<PipelineBase>> pipelines_;
};

}  // namespace gpu

#endif  // PLATFORM_MEDIA_GPU_PIPELINE_PROPMEDIA_GPU_CHANNEL_H_
