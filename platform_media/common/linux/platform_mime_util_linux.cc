// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
//
// This file is an original work developed by Vivaldi Technologies AS.

#include "platform_media/common/platform_mime_util.h"

namespace media {

bool IsPlatformMediaPipelineAvailable(PlatformMediaCheckType) {
  return false;
}

bool IsPlatformAudioDecoderAvailable(AudioCodec) {
  return false;
}

bool IsPlatformVideoDecoderAvailable() {
  return false;
}

}  // namespace media
