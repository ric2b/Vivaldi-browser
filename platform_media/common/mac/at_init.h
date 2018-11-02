// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2015 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#ifndef PLATFORM_MEDIA_COMMON_MAC_AT_INIT_H_
#define PLATFORM_MEDIA_COMMON_MAC_AT_INIT_H_

#include "platform_media/common/feature_toggles.h"

namespace media {

// Calls the minimum amount of the AudioToolbox API with the sole purpose of
// warming up the sandbox for audio decoding.
void InitializeAudioToolbox();

}  // namespace media

#endif  // PLATFORM_MEDIA_COMMON_MAC_AT_INIT_H_
