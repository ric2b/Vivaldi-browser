// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (C) 2015 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#ifndef MEDIA_BASE_MAC_SCOPED_AUDIO_QUEUE_REF_H_
#define MEDIA_BASE_MAC_SCOPED_AUDIO_QUEUE_REF_H_

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

#endif  // MEDIA_BASE_MAC_SCOPED_AUDIO_QUEUE_REF_H_
