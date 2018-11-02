// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA.

#ifndef PLATFORM_MEDIA_GPU_DECODERS_MAC_AVF_AUDIO_TAP_H_
#define PLATFORM_MEDIA_GPU_DECODERS_MAC_AVF_AUDIO_TAP_H_

#include "platform_media/common/feature_toggles.h"

#import <CoreAudio/CoreAudio.h>

#include "base/callback.h"
#include "base/mac/scoped_nsobject.h"

@class AVAssetTrack;
@class AVAudioMix;

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace media {
class DataBuffer;
}  // namespace media

namespace media {

// Used to grab decoded audio samples from an AVPlayerItem, see
// |GetAudioMix()|.
class AVFAudioTap {
 public:
  using FormatKnownCB =
      base::Callback<void(const AudioStreamBasicDescription& format)>;
  using SamplesReadyCB =
      base::Callback<void(const scoped_refptr<DataBuffer>& buffer)>;

  AVFAudioTap(AVAssetTrack* audio_track,
              const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
              const FormatKnownCB& format_known_cb,
              const SamplesReadyCB& samples_ready_cb);
  ~AVFAudioTap();

  // Returns an AVAudioMix with an audio processing tap attached to it.  Set
  // the AVAudioMix on an AVPlayerItem to receive decoded audio samples through
  // |samples_ready_cb_|.  Returns nil on error to initialize the AVAudioMix.
  base::scoped_nsobject<AVAudioMix> GetAudioMix();

 private:
  AVAssetTrack* audio_track_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  FormatKnownCB format_known_cb_;
  SamplesReadyCB samples_ready_cb_;

  DISALLOW_COPY_AND_ASSIGN(AVFAudioTap);
};

}  // namespace media

#endif  // PLATFORM_MEDIA_GPU_DECODERS_MAC_AVF_AUDIO_TAP_H_
