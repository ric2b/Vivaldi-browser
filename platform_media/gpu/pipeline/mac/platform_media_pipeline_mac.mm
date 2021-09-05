// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#include "platform_media/gpu/pipeline/platform_media_pipeline_factory.h"

#include "platform_media/gpu/pipeline/mac/avf_media_pipeline.h"
#include "platform_media/gpu/pipeline/mac/avf_media_reader_runner.h"

#include "base/logging.h"

namespace media {

namespace {

class AVFMediaPipelineFactory : public PlatformMediaPipelineFactory {
 public:
  std::unique_ptr<PlatformMediaPipeline> CreatePipeline() override {
    VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__;
    if (AVFMediaReaderRunner::IsAvailable())
      return std::make_unique<AVFMediaReaderRunner>();

    return std::make_unique<AVFMediaPipeline>();
  }
};

}  // namespace

// static
std::unique_ptr<PlatformMediaPipelineFactory>
PlatformMediaPipelineFactory::Create() {
  VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__;
  return std::make_unique<AVFMediaPipelineFactory>();
}

}  // namespace media
