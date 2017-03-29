// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (C) 2015 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA.

#include "media/base/platform_mime_util.h"

#include "base/mac/mac_util.h"

namespace media {

bool IsPlatformMediaPipelineAvailable(PlatformMediaCheckType /* check_type */) {
  // AVAssetResourceLoaderDelegate is required.
  return base::mac::IsOSMavericksOrLater();
}

bool IsPlatformAudioDecoderAvailable() {
  return true;
}

bool IsPlatformVideoDecoderAvailable() {
  // VTVideoDecodeAccelerator currently requires 10.9.
  return base::mac::IsOSMavericksOrLater();
}

}  // namespace media
