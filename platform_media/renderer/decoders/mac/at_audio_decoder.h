// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2015 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA.

#ifndef PLATFORM_MEDIA_RENDERER_DECODERS_MAC_AT_AUDIO_DECODER_H_
#define PLATFORM_MEDIA_RENDERER_DECODERS_MAC_AT_AUDIO_DECODER_H_

#include "platform_media/common/feature_toggles.h"

#include <AudioToolbox/AudioConverter.h>

#include <deque>
#include <string>

#include "base/mac/scoped_typeref.h"
#include "media/base/audio_decoder.h"
#include "platform_media/renderer/decoders/debug_buffer_logger.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace media {

class AudioDiscardHelper;

// An AudioDecoder that uses Apple's AudioToolbox to decode audio.
class ATAudioDecoder : public AudioDecoder {
 public:
  struct FormatDetection;

  explicit ATAudioDecoder(scoped_refptr<base::SequencedTaskRunner> task_runner);
  ~ATAudioDecoder() override;
  ATAudioDecoder(const ATAudioDecoder&) = delete;
  ATAudioDecoder& operator=(const ATAudioDecoder&) = delete;

  // AudioDecoder implementation.
  AudioDecoderType GetDecoderType() const override;
  void Initialize(const AudioDecoderConfig& config,
                  CdmContext* cdm_context,
                  InitCB init_cb,
                  const OutputCB& output_cb,
                  const WaitingCB& waiting_for_decryption_key_cb) override;
  void Decode(scoped_refptr<DecoderBuffer> buffer, DecodeCB decode_cb) override;
  void Reset(base::OnceClosure closure) override;

 private:
  bool InitializeConverter(const AudioStreamBasicDescription& input_format,
                           const std::vector<uint8_t>& full_extra_data);
  void CloseConverter();

  bool DetectFormat(scoped_refptr<DecoderBuffer> buffer);
  bool ConvertAudio(scoped_refptr<DecoderBuffer> buffer);

  void ResetTimestampState();

  const scoped_refptr<base::SequencedTaskRunner> task_runner_;

  AudioDecoderConfig config_;
  std::unique_ptr<FormatDetection> format_detection_;
  AudioStreamBasicDescription output_format_;
  AudioConverterRef converter_ = nullptr;
  std::deque<DecoderBuffer::TimeInfo> queued_input_timing_;
  std::unique_ptr<AudioDiscardHelper> discard_helper_;
  OutputCB output_cb_;

  DebugBufferLogger debug_buffer_logger_;
};

}  // namespace media

#endif  // PLATFORM_MEDIA_RENDERER_DECODERS_MAC_AT_AUDIO_DECODER_H_
