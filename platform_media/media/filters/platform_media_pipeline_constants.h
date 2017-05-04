// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (C) 2015 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#ifndef MEDIA_FILTERS_PLATFORM_MEDIA_PIPELINE_CONSTANTS_H_
#define MEDIA_FILTERS_PLATFORM_MEDIA_PIPELINE_CONSTANTS_H_

#include <stdint.h>

#include "gpu/command_buffer/common/gles2_cmd_format.h"
#include "media/base/media_export.h"

namespace media {

MEDIA_EXPORT extern const GLenum kPlatformMediaPipelineTextureFormat;
MEDIA_EXPORT extern const uint32_t kPlatformMediaPipelineTextureTarget;

}  // namespace media

#endif  // MEDIA_FILTERS_PLATFORM_MEDIA_PIPELINE_CONSTANTS_H_
