// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#include "platform_media/gpu/pipeline/platform_media_pipeline_create.h"
#include "platform_media/gpu/pipeline/platform_media_pipeline.h"

#include "platform_media/gpu/data_source/ipc_data_source.h"
#include "platform_media/gpu/pipeline/win/wmf_media_pipeline.h"

namespace media {

// static
PlatformMediaPipeline* PlatformMediaPipelineCreate(
    IPCDataSource* data_source,
    const PlatformMediaPipeline::AudioConfigChangedCB& audio_config_changed_cb,
    const PlatformMediaPipeline::VideoConfigChangedCB& video_config_changed_cb) {
  return new WMFMediaPipeline(
      data_source, audio_config_changed_cb, video_config_changed_cb);
}

}  // namespace media
