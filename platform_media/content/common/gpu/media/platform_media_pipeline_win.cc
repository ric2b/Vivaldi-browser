// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#include "content/common/gpu/media/platform_media_pipeline.h"

#include "content/common/gpu/media/ipc_data_source.h"
#include "content/common/gpu/media/wmf_media_pipeline.h"

namespace content {

// static
PlatformMediaPipeline* PlatformMediaPipeline::Create(
    IPCDataSource* data_source,
    const AudioConfigChangedCB& audio_config_changed_cb,
    const VideoConfigChangedCB& video_config_changed_cb,
    media::PlatformMediaDecodingMode preferred_video_decoding_mode,
    const MakeGLContextCurrentCB& make_gl_context_current_cb) {
  return new WMFMediaPipeline(
      data_source, audio_config_changed_cb, video_config_changed_cb,
      preferred_video_decoding_mode, make_gl_context_current_cb);
}

// static
bool PlatformMediaPipeline::EnlargesBuffersOnUnderflow() {
  return false;
}

// static
base::TimeDelta PlatformMediaPipeline::GetTargetBufferDurationBehind() {
  return base::TimeDelta();
}

// static
base::TimeDelta PlatformMediaPipeline::GetTargetBufferDurationAhead() {
  return base::TimeDelta();
}

}  // namespace content
