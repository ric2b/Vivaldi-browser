// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#include "content/common/gpu/media/platform_media_pipeline.h"

#include "content/common/gpu/media/avf_media_pipeline.h"
#include "content/common/gpu/media/avf_media_reader_runner.h"

namespace content {

// static
PlatformMediaPipeline* PlatformMediaPipeline::Create(
    IPCDataSource* data_source,
    const AudioConfigChangedCB& /* audio_config_changed_cb */,
    const VideoConfigChangedCB& /* video_config_changed_cb */,
    media::PlatformMediaDecodingMode /* preferred_video_decoding_mode */,
    const MakeGLContextCurrentCB& /* make_gl_context_current_cb */) {
  if (AVFMediaReaderRunner::IsAvailable())
    return new AVFMediaReaderRunner(data_source);

  return new AVFMediaPipeline(data_source);
}

// static
bool PlatformMediaPipeline::EnlargesBuffersOnUnderflow() {
  return !AVFMediaReaderRunner::IsAvailable();
}

// static
base::TimeDelta PlatformMediaPipeline::GetTargetBufferDurationBehind() {
  // AV Foundation is known to make requests for "past" data quite often.  We
  // use a large "behind buffer" to prevent cache misses.
  return base::TimeDelta::FromSeconds(20);
}

// static
base::TimeDelta PlatformMediaPipeline::GetTargetBufferDurationAhead() {
  return base::TimeDelta();
}

}  // namespace content
