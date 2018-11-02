// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_MEDIA_GPU_PIPELINE_PROPMEDIA_GPU_CHANNEL_MANAGER_H_
#define PLATFORM_MEDIA_GPU_PIPELINE_PROPMEDIA_GPU_CHANNEL_MANAGER_H_

#include "platform_media/common/feature_toggles.h"

#include <map>
#include <memory>
#include <unordered_map>

#include "base/unguessable_token.h"

namespace gpu {

class GpuChannelManager;
class ProprietaryMediaGpuChannel;

class ProprietaryMediaGpuChannelManager
{
 public:

  explicit ProprietaryMediaGpuChannelManager(gpu::GpuChannelManager* manager);
  ~ProprietaryMediaGpuChannelManager();
  void AddChannel(int32_t client_id);
  void RemoveChannel(int32_t client_id);

 private:

  gpu::GpuChannelManager* const channel_manager_;
  std::unordered_map<int32_t, std::unique_ptr<ProprietaryMediaGpuChannel>>
      media_gpu_channels_;
  std::map<base::UnguessableToken, int32_t> token_to_channel_;
  std::map<int32_t, base::UnguessableToken> channel_to_token_;

  DISALLOW_COPY_AND_ASSIGN(ProprietaryMediaGpuChannelManager);
};

}  // namespace gpu

#endif  // PLATFORM_MEDIA_GPU_PIPELINE_PROPMEDIA_GPU_CHANNEL_MANAGER_H_
