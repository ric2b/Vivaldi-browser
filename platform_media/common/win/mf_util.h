// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2015 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA.

#ifndef PLATFORM_MEDIA_COMMON_WIN_MF_UTIL_H_
#define PLATFORM_MEDIA_COMMON_WIN_MF_UTIL_H_

#include "platform_media/common/feature_toggles.h"

#if !defined(USE_SYSTEM_PROPRIETARY_CODECS)
#error Should only be built with USE_SYSTEM_PROPRIETARY_CODECS
#endif

#include <windows.h>

#include <string>

#include "media/base/audio_decoder_config.h"
#include "media/base/media_export.h"

namespace media {

// Utility functions for loading different subsets of the Media Foundation
// libraries.  In each case, the library name appropriate for the current
// Windows version is chosen.  The functions can be called many times per
// process and return the same value throughout the lifetime of the process.
MEDIA_EXPORT bool LoadMFCommonLibraries();
MEDIA_EXPORT void LoadMFAudioDecoderLibraries();
MEDIA_EXPORT bool LoadMFAudioDecoderLibrary(AudioCodec codec);
MEDIA_EXPORT bool LoadMFVideoDecoderLibraries();
MEDIA_EXPORT bool LoadMFSourceReaderLibraries();

MEDIA_EXPORT std::string GetMFAudioDecoderLibraryName(AudioCodec codec);
MEDIA_EXPORT std::string GetMFVideoDecoderLibraryName();

MEDIA_EXPORT FARPROC
GetFunctionFromLibrary(const char* function_name, const char* library_name);

}  // namespace media

#endif  // PLATFORM_MEDIA_COMMON_WIN_MF_UTIL_H_
