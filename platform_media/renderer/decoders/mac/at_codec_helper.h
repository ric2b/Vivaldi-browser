// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2015 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA.

#ifndef PLATFORM_MEDIA_RENDERER_DECODERS_MAC_AT_CODEC_HELPER_H_
#define PLATFORM_MEDIA_RENDERER_DECODERS_MAC_AT_CODEC_HELPER_H_

#include "platform_media/common/feature_toggles.h"

#include "base/callback.h"
#include "base/memory/free_deleter.h"

struct AudioChannelLayout;
struct AudioStreamBasicDescription;

namespace media {

class AudioDecoderConfig;
class DecoderBuffer;

// Responsible for codec-specific tasks of an audio decoder based on Apple's
// AudioToolbox.
class ATCodecHelper {
 public:
  using ScopedAudioChannelLayoutPtr =
      std::unique_ptr<AudioChannelLayout, base::FreeDeleter>;

  // Invoked when there is enough information about the audio stream to
  // determine the exact format.  Returns false on failure.
  using InputFormatKnownCB =
      base::Callback<bool(const AudioStreamBasicDescription& format,
                          ScopedAudioChannelLayoutPtr input_channel_layout)>;

  // Invoked every time a DecoderBuffer should be converted to an AudioBuffer.
  // |header_size| is the number of bytes to be discarded from the beginning of
  // |input|.  |max_output_frame_count| specifies the maximum expected number
  // of frames of decoded audio.  Returns false on failure.
  using ConvertAudioCB =
      base::Callback<bool(const scoped_refptr<DecoderBuffer>& input,
                          size_t header_size,
                          size_t max_output_frame_count)>;

  virtual ~ATCodecHelper() {}

  // The callbacks must be invoked synchronously either within Initialize() or
  // ProcessBuffer().  In particular, |input_format_known_cb| is _not_ required
  // to be invoked within Initialize() -- some codecs require some parsing of
  // the actual audio stream to determine the exact input format.
  virtual bool Initialize(const AudioDecoderConfig& config,
                          const InputFormatKnownCB& input_format_known_cb,
                          const ConvertAudioCB& convert_audio_cb) = 0;

  virtual bool ProcessBuffer(const scoped_refptr<DecoderBuffer>& buffer) = 0;
};

}  // namespace media

#endif  // PLATFORM_MEDIA_RENDERER_DECODERS_MAC_AT_CODEC_HELPER_H_
