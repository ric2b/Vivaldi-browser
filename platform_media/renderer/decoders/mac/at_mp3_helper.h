// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2015 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA.

#ifndef PLATFORM_MEDIA_RENDERER_DECODERS_MAC_AT_MP3_HELPER_H_
#define PLATFORM_MEDIA_RENDERER_DECODERS_MAC_AT_MP3_HELPER_H_

#include "platform_media/common/feature_toggles.h"

#include "platform_media/renderer/decoders/mac/at_codec_helper.h"

namespace media {

class ATMP3Helper final : public ATCodecHelper {
 public:
  ATMP3Helper();
  ~ATMP3Helper() override;

  bool Initialize(const AudioDecoderConfig& config,
                  const InputFormatKnownCB& input_format_known_cb,
                  const ConvertAudioCB& convert_audio_cb) override;
  bool ProcessBuffer(const scoped_refptr<DecoderBuffer>& buffer) override;

 private:
  ConvertAudioCB convert_audio_cb_;

  DISALLOW_COPY_AND_ASSIGN(ATMP3Helper);
};

}  // namespace media

#endif  // PLATFORM_MEDIA_RENDERER_DECODERS_MAC_AT_MP3_HELPER_H_
