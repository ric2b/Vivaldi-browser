// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (C) 2015 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA.

#ifndef MEDIA_BASE_PLATFORM_MIME_UTIL_H_
#define MEDIA_BASE_PLATFORM_MIME_UTIL_H_

#if !defined(USE_SYSTEM_PROPRIETARY_CODECS)
#error Should only be built with USE_SYSTEM_PROPRIETARY_CODECS
#endif

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
bool IsPlatformAudioDecoderAvailable();

// Returns true iff the system is able to decode video streams using system
// libraries.
bool IsPlatformVideoDecoderAvailable();

}  // namespace media

#endif  // MEDIA_BASE_PLATFORM_MIME_UTIL_H_
