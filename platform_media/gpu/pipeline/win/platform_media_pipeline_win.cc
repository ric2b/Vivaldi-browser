// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#include "platform_media/gpu/pipeline/platform_media_pipeline_create.h"
#include "platform_media/gpu/pipeline/platform_media_pipeline.h"

#include "platform_media/gpu/data_source/ipc_data_source.h"
#include "platform_media/gpu/pipeline/win/wmf_media_pipeline.h"

namespace content {

// static
PlatformMediaPipeline* PlatformMediaPipelineCreate(
    IPCDataSource* data_source,
    const PlatformMediaPipeline::AudioConfigChangedCB& audio_config_changed_cb,
    const PlatformMediaPipeline::VideoConfigChangedCB& video_config_changed_cb,
    media::PlatformMediaDecodingMode preferred_video_decoding_mode,
    const PlatformMediaPipeline::MakeGLContextCurrentCB& make_gl_context_current_cb) {
  return new WMFMediaPipeline(
      data_source, audio_config_changed_cb, video_config_changed_cb,
      preferred_video_decoding_mode, make_gl_context_current_cb);
}

}  // namespace content
