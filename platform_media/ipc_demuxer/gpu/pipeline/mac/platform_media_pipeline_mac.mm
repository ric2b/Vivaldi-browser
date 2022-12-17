// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#include "platform_media/ipc_demuxer/gpu/pipeline/platform_media_pipeline.h"

#include "platform_media/ipc_demuxer/gpu/pipeline/mac/avf_media_reader_runner.h"

namespace media {

std::unique_ptr<PlatformMediaPipeline> PlatformMediaPipeline::Create() {
  return std::make_unique<AVFMediaReaderRunner>();
}

}  // namespace media
