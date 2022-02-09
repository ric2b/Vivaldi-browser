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

#include "base/win/windows_types.h"

#include "media/base/media_export.h"

namespace media {

// Try to load common MF DLLs. This should be called once per process prior
// sandbox initialization. With demuxer_support load all libraries for media
// demultiplexing.
MEDIA_EXPORT void LoadMFDecodingLibraries(bool demuxer_support);

// Get audio decoding library. Must be called after calling
// LoadMFCommonLibraries().
HMODULE GetMFAudioDecoderLibrary();

// Get video decoding library. Must be called after calling
// LoadMFCommonLibraries().
HMODULE GetMFVideoDecoderLibrary();

// Return true if media demuxer support is available. Must be called after
// calling LoadMFCommonLibraries().
bool HasMFDemuxerSupport();

}  // namespace media

#endif  // PLATFORM_MEDIA_COMMON_WIN_MF_UTIL_H_
