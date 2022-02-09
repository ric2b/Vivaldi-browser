// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#include "platform_media/gpu/pipeline/platform_media_pipeline.h"

#include "platform_media/gpu/pipeline/mac/avf_media_pipeline.h"
#include "platform_media/gpu/pipeline/mac/avf_media_reader_runner.h"

namespace media {

std::unique_ptr<PlatformMediaPipeline> PlatformMediaPipeline::Create() {
  if (AVFMediaReaderRunner::IsAvailable())
    return std::make_unique<AVFMediaReaderRunner>();

  return std::make_unique<AVFMediaPipeline>();
}

}  // namespace media
