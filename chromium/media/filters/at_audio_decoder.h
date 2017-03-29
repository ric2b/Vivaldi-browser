// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (C) 2015 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA.

#ifndef MEDIA_FILTERS_AT_AUDIO_DECODER_H_
#define MEDIA_FILTERS_AT_AUDIO_DECODER_H_

#include <AudioToolbox/AudioConverter.h>

#include <string>

#include "base/mac/scoped_typeref.h"
#include "media/base/audio_decoder.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace media {

// An AudioDecoder that uses Apple's AudioToolbox to decode audio.
class MEDIA_EXPORT ATAudioDecoder : public AudioDecoder {
 public:
  explicit ATAudioDecoder(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner);
  ~ATAudioDecoder() override;

  // AudioDecoder implementation.
  std::string GetDisplayName() const override;
  void Initialize(const AudioDecoderConfig& config,
                  const InitCB& init_cb,
                  const OutputCB& output_cb) override;
  void Decode(const scoped_refptr<DecoderBuffer>& buffer,
              const DecodeCB& decode_cb) override;
  void Reset(const base::Closure& closure) override;

 private:
  class AudioFormatReader;

  struct ScopedAudioConverterRefTraits {
    static void Retain(AudioConverterRef converter);
    static void Release(AudioConverterRef converter);
  };
  using ScopedAudioConverterRef =
      base::ScopedTypeRef<AudioConverterRef, ScopedAudioConverterRefTraits>;

  bool ReadInputChannelLayoutFromEsds(const AudioDecoderConfig& config);
  bool MaybeInitializeConverter(const scoped_refptr<DecoderBuffer>& buffer);
  bool ConvertAudio(const scoped_refptr<DecoderBuffer>& input);

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  AudioDecoderConfig config_;
  scoped_ptr<AudioChannelLayout, base::FreeDeleter> input_channel_layout_;
  scoped_ptr<AudioFormatReader> input_format_reader_;
  ScopedAudioConverterRef converter_;
  OutputCB output_cb_;

  DISALLOW_COPY_AND_ASSIGN(ATAudioDecoder);
};

}  // namespace media

#endif  // MEDIA_FILTERS_AT_AUDIO_DECODER_H_
