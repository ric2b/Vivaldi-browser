// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
//
// This file is an original work developed by Vivaldi Technologies AS.

#ifndef PLATFORM_MEDIA_COMMON_PLATFORM_LOGGING_UTIL_H_
#define PLATFORM_MEDIA_COMMON_PLATFORM_LOGGING_UTIL_H_

#include "platform_media/common/feature_toggles.h"

#include "platform_media/common/platform_media_pipeline_types.h"

#include "media/base/media_export.h"

#include <string>

namespace media {

class AudioDecoderConfig;
class VideoDecoderConfig;
struct PlatformAudioConfig;
struct PlatformVideoConfig;

MEDIA_EXPORT std::string Loggable(const PlatformVideoConfig & config);

MEDIA_EXPORT std::string Loggable(const VideoDecoderConfig & config);

MEDIA_EXPORT std::string Loggable(const AudioDecoderConfig & config);

MEDIA_EXPORT std::string Loggable(const PlatformAudioConfig & config);

MEDIA_EXPORT std::string LoggableMediaType(PlatformMediaDataType type);

}

#endif  // PLATFORM_MEDIA_COMMON_PLATFORM_LOGGING_UTIL_H_
