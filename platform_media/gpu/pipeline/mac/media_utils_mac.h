// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#ifndef PLATFORM_MEDIA_GPU_PIPELINE_MAC_MEDIA_UTILS_MAC_H_
#define PLATFORM_MEDIA_GPU_PIPELINE_MAC_MEDIA_UTILS_MAC_H_

#include "platform_media/common/feature_toggles.h"

#include "platform_media/common/platform_media_pipeline_types.h"

struct AudioStreamBasicDescription;
@class AVAssetTrack;
typedef const struct opaqueCMFormatDescription* CMFormatDescriptionRef;

namespace media {

base::TimeDelta MEDIA_EXPORT GetStartTimeFromTrack(AVAssetTrack* track);

PlatformVideoConfig MEDIA_EXPORT
GetPlatformVideoConfig(CMFormatDescriptionRef description,
                       CGAffineTransform transform,
                       size_t stride_Y = 0,
                       size_t stride_UV = 0);

}  // namespace media

#endif  // PLATFORM_MEDIA_GPU_PIPELINE_MAC_MEDIA_UTILS_MAC_H_
