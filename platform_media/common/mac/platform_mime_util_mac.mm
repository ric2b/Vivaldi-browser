// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2015 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA.

#include "platform_media/common/platform_mime_util.h"

#include "base/mac/mac_util.h"

namespace media {

bool IsPlatformMediaPipelineAvailable(PlatformMediaCheckType /* check_type */) {
  return true;
}

bool IsPlatformAudioDecoderAvailable(AudioCodec /* codec */) {
  return true;
}

bool IsPlatformVideoDecoderAvailable() {
  return true;
}

}  // namespace media
