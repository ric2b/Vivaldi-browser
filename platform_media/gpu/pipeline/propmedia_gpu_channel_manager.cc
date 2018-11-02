// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform_media/gpu/pipeline/propmedia_gpu_channel_manager.h"
#include "platform_media/gpu/pipeline/propmedia_gpu_channel.h"

#include "base/memory/ptr_util.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "gpu/ipc/service/gpu_channel_manager.h"

namespace gpu {

ProprietaryMediaGpuChannelManager::ProprietaryMediaGpuChannelManager(
    gpu::GpuChannelManager* channel_manager)
    : channel_manager_(channel_manager) {}

ProprietaryMediaGpuChannelManager::~ProprietaryMediaGpuChannelManager() {}

void ProprietaryMediaGpuChannelManager::AddChannel(int32_t client_id) {
  gpu::GpuChannel* gpu_channel = channel_manager_->LookupChannel(client_id);
  DCHECK(gpu_channel);

  std::unique_ptr<ProprietaryMediaGpuChannel> media_gpu_channel(
      new ProprietaryMediaGpuChannel(gpu_channel));
  gpu_channel->SetProprietaryMediaMessageListener(media_gpu_channel.get());

  base::UnguessableToken channel_token = base::UnguessableToken::Create();
  media_gpu_channels_[client_id] = std::move(media_gpu_channel);
  channel_to_token_[client_id] = channel_token;
  token_to_channel_[channel_token] = client_id;
}

void ProprietaryMediaGpuChannelManager::RemoveChannel(int32_t client_id) {
  media_gpu_channels_.erase(client_id);
  const auto it = channel_to_token_.find(client_id);
  if (it != channel_to_token_.end()) {
    token_to_channel_.erase(it->second);
    channel_to_token_.erase(it);
  }
}

}  // namespace gpu
