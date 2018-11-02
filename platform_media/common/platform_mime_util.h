// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2015 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA.

#ifndef PLATFORM_MEDIA_COMMON_PLATFORM_MIME_UTIL_H_
#define PLATFORM_MEDIA_COMMON_PLATFORM_MIME_UTIL_H_

#include "platform_media/common/feature_toggles.h"

#if !defined(USE_SYSTEM_PROPRIETARY_CODECS)
#error Should only be built with USE_SYSTEM_PROPRIETARY_CODECS
#endif

#include "media/base/audio_decoder_config.h"
#include "media/base/media_export.h"

namespace media {

enum class PlatformMediaCheckType {
  // Instructs to perform basic availability checks.  The result should be
  // close to the |FULL| result in most cases, but it's allowed to be overly
  // optimistic.  This type is intended for callers with restricted privileges,
  // e.g., those running in the renderer process.
  BASIC,
  // Instructs to perform full availability checks.
  FULL,
};

// Returns true iff the system is able to demux media files and return decoded
// audio and video streams using system libraries.
MEDIA_EXPORT bool IsPlatformMediaPipelineAvailable(
    PlatformMediaCheckType check_type);

// Returns true iff the system is able to decode audio streams using system
// libraries.
bool IsPlatformAudioDecoderAvailable(AudioCodec codec);

// Returns true iff the system is able to decode video streams using system
// libraries.
bool IsPlatformVideoDecoderAvailable();

}  // namespace media

#endif  // PLATFORM_MEDIA_COMMON_PLATFORM_MIME_UTIL_H_
