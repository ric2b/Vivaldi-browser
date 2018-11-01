// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (C) 2015 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA.

#include "platform_media/renderer/decoders/mac/at_mp3_helper.h"

#include <CoreAudio/CoreAudio.h>

#include "media/base/audio_decoder_config.h"
#include "platform_media/common/mac/framework_type_conversions.h"

namespace media {

ATMP3Helper::ATMP3Helper() = default;

ATMP3Helper::~ATMP3Helper() = default;

bool ATMP3Helper::Initialize(const AudioDecoderConfig& config,
                             const InputFormatKnownCB& input_format_known_cb,
                             const ConvertAudioCB& convert_audio_cb) {
  convert_audio_cb_ = convert_audio_cb;

  AudioStreamBasicDescription format = {0};
  format.mSampleRate = config.input_samples_per_second();
  format.mFormatID = kAudioFormatMPEGLayer3;
  format.mChannelsPerFrame =
      ChannelLayoutToChannelCount(config.channel_layout());

  ScopedAudioChannelLayoutPtr channel_layout(
      static_cast<AudioChannelLayout*>(malloc(sizeof(AudioChannelLayout))));
  memset(channel_layout.get(), 0, sizeof(AudioChannelLayout));
  channel_layout->mChannelLayoutTag =
      ChromeChannelLayoutToCoreAudioTag(config.channel_layout());

  return input_format_known_cb.Run(format, std::move(channel_layout));
}

bool ATMP3Helper::ProcessBuffer(const scoped_refptr<DecoderBuffer>& buffer) {
  // http://teslabs.com/openplayer/docs/docs/specs/mp3_structure2.pdf
  const size_t kMaxOutputFrameCount = 1152;

  return convert_audio_cb_.Run(buffer, 0, kMaxOutputFrameCount);
}

}  // namespace media
