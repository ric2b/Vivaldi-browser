// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2015 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#include "platform_media/common/mac/at_init.h"

#include "platform_media/common/mac/scoped_audio_queue_ref.h"

namespace media {

namespace {

void DummyOutputCallback(void* inUserData,
                         AudioQueueRef inAQ,
                         AudioQueueBufferRef inBuffer) {
  NOTREACHED();
}

}  // namespace

void InitializeAudioToolbox() {
  // Create and start a dummy AudioQueue to preload the resources used when
  // decoding audio.
  AudioStreamBasicDescription format;
  memset(&format, 0, sizeof(format));
  format.mFormatID = '.mp3';
  format.mSampleRate = 44100;
  format.mChannelsPerFrame = 2;

  ScopedAudioQueueRef queue;
  AudioQueueNewOutput(&format, &DummyOutputCallback, nullptr, nullptr, nullptr,
                      0, queue.InitializeInto());
  if (queue) {
    if (AudioQueueStart(queue, nullptr) == noErr)
      AudioQueueStop(queue, true);
  }
}

}  // namespace media
