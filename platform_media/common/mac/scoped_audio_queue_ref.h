// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2015 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#ifndef PLATFORM_MEDIA_COMMON_MAC_SCOPED_AUDIO_QUEUE_REF_H_
#define PLATFORM_MEDIA_COMMON_MAC_SCOPED_AUDIO_QUEUE_REF_H_

#include "platform_media/common/feature_toggles.h"

#include <AudioToolbox/AudioQueue.h>

#include "base/mac/scoped_typeref.h"

namespace media {

struct ScopedAudioQueueRefTraits {
  static AudioQueueRef Retain(AudioQueueRef queue) {
    NOTREACHED() << "Only compatible with ASSUME policy";
    return queue;
  }
  static void Release(AudioQueueRef queue) { AudioQueueDispose(queue, true); }
  static AudioQueueRef InvalidValue() { return nullptr; }
};

using ScopedAudioQueueRef =
    base::ScopedTypeRef<AudioQueueRef, ScopedAudioQueueRefTraits>;

}  // namespace media

#endif  // PLATFORM_MEDIA_COMMON_MAC_SCOPED_AUDIO_QUEUE_REF_H_
