// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (C) 2015 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA.

#include "media/base/platform_mime_util.h"

#include "media/base/win/mf_util.h"

namespace media {

bool IsPlatformMediaPipelineAvailable(PlatformMediaCheckType check_type) {
  if (!LoadMFCommonLibraries())
    return false;

  if (check_type == PlatformMediaCheckType::FULL &&
      !LoadMFSourceReaderLibraries())
    return false;

  return true;
}

bool IsPlatformAudioDecoderAvailable(AudioCodec codec) {
  return LoadMFCommonLibraries() && LoadMFAudioDecoderLibrary(codec);
}

bool IsPlatformVideoDecoderAvailable() {
  return LoadMFCommonLibraries() && LoadMFVideoDecoderLibraries();
}

}  // namespace media
