// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#include "media/filters/platform_media_pipeline_types.h"

namespace media {

PlatformVideoConfig::PlatformVideoConfig()
    : rotation(VIDEO_ROTATION_0),
      decoding_mode(PlatformMediaDecodingMode::SOFTWARE) {}

PlatformVideoConfig::PlatformVideoConfig(const PlatformVideoConfig& other) = default;

PlatformVideoConfig::~PlatformVideoConfig() {
}

}  // namespace media
