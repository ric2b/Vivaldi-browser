// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#include "platform_media/gpu/pipeline/platform_media_pipeline_create.h"
#include "platform_media/gpu/pipeline/platform_media_pipeline.h"

#include "platform_media/gpu/pipeline/mac/avf_media_pipeline.h"
#include "platform_media/gpu/pipeline/mac/avf_media_reader_runner.h"

namespace media {

// static
PlatformMediaPipeline* PlatformMediaPipelineCreate(
    IPCDataSource* data_source,
    const PlatformMediaPipeline::AudioConfigChangedCB& /* audio_config_changed_cb */,
    const PlatformMediaPipeline::VideoConfigChangedCB& /* video_config_changed_cb */) {

  VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__;
  if (AVFMediaReaderRunner::IsAvailable())
    return new AVFMediaReaderRunner(data_source);

  return new AVFMediaPipeline(data_source);
}

}  // namespace media
