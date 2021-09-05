// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#include "platform_media/gpu/pipeline/platform_media_pipeline_factory.h"

#include "platform_media/gpu/pipeline/win/wmf_media_pipeline.h"

#include "base/logging.h"
#include "media/base/win/mf_initializer.h"

namespace media {

namespace {

class WMFMediaPipelineFactory : public PlatformMediaPipelineFactory {
 public:
  std::unique_ptr<PlatformMediaPipeline> CreatePipeline() override {
    VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__;
    if (!session_) {
      session_ = InitializeMediaFoundation();
      if (!session_) {
        LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__
                   << " Failed to initialize Media Foundation ";
        return nullptr;
      }
    }
    return std::make_unique<WMFMediaPipeline>();
  }

 private:
  MFSessionLifetime session_;
};

}  // namespace

// static
std::unique_ptr<PlatformMediaPipelineFactory>
PlatformMediaPipelineFactory::Create() {
  VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__;
  return std::make_unique<WMFMediaPipelineFactory>();
}

}  // namespace media
