// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (C) 2015 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#ifndef CONTENT_COMMON_GPU_MEDIA_AT_INIT_H_
#define CONTENT_COMMON_GPU_MEDIA_AT_INIT_H_

namespace content {

// Calls the minimum amount of the AudioToolbox API with the sole purpose of
// warming up the sandbox for audio decoding.
void InitializeAudioToolbox();

}  // namespace content

#endif  // CONTENT_COMMON_GPU_MEDIA_AT_INIT_H_
