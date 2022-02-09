// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#include "platform_media/gpu/pipeline/platform_media_pipeline.h"

#include "base/logging.h"
#include "media/base/win/mf_initializer.h"

#include "platform_media/gpu/pipeline/win/wmf_media_pipeline.h"

namespace media {

/* static */
std::unique_ptr<PlatformMediaPipeline> PlatformMediaPipeline::Create() {
  if (!InitializeMediaFoundation()) {
    LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__
               << " Failed to initialize Media Foundation ";
    return nullptr;
  }
  return std::make_unique<WMFMediaPipeline>();
}

}  // namespace media
