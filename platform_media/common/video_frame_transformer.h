// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2015 Opera Software ASA.  All rights reserved.
//
// This file is based on original work developed by Opera Software ASA.

#ifndef PLATFORM_MEDIA_COMMON_VIDEO_FRAME_TRANSFORMER_H_
#define PLATFORM_MEDIA_COMMON_VIDEO_FRAME_TRANSFORMER_H_

#include "platform_media/common/feature_toggles.h"

#include "base/memory/ref_counted.h"

#include "media/base/media_export.h"

namespace media {

class DecoderBuffer;
class VideoDecoderConfig;
class VideoFrame;

MEDIA_EXPORT scoped_refptr<VideoFrame> GetVideoFrameFromMemory(
    const scoped_refptr<DecoderBuffer>& buffer,
    const VideoDecoderConfig& config);

} // namespace media

#endif  // PLATFORM_MEDIA_COMMON_VIDEO_FRAME_TRANSFORMER_H_
