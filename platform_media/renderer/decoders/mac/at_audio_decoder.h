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
#include "platform_media/renderer/decoders/mac/at_codec_helper.h"
#include "platform_media/renderer/decoders/debug_buffer_logger.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace media {

class AudioDiscardHelper;

// An AudioDecoder that uses Apple's AudioToolbox to decode audio.
//
// Once initialized, the decoding can proceed in the same manner for all
// currently supported codecs.  The initialization part is highly
// codec-dependent, though.  That's why this part of the decoder is moved to
// dedicated implementations of ATCodecHelper.
class MEDIA_EXPORT ATAudioDecoder : public AudioDecoder {
 public:
  explicit ATAudioDecoder(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner);
  ~ATAudioDecoder() override;

  // AudioDecoder implementation.
  std::string GetDisplayName() const override;
  void Initialize(const AudioDecoderConfig& config,
                  CdmContext* cdm_context,
                  const InitCB& init_cb,
                  const OutputCB& output_cb,
                  const WaitingForDecryptionKeyCB& waiting_for_decryption_key_cb) override;
  void Decode(scoped_refptr<DecoderBuffer> buffer,
              const DecodeCB& decode_cb) override;
  void Reset(const base::Closure& closure) override;

 private:
  using ScopedAudioChannelLayoutPtr =
      ATCodecHelper::ScopedAudioChannelLayoutPtr;

  struct ScopedAudioConverterRefTraits {
    static AudioConverterRef Retain(AudioConverterRef converter);
    static void Release(AudioConverterRef converter);
    static AudioConverterRef InvalidValue();
  };
  using ScopedAudioConverterRef =
      base::ScopedTypeRef<AudioConverterRef, ScopedAudioConverterRefTraits>;

  bool InitializeConverter(const AudioStreamBasicDescription& input_format,
                           ScopedAudioChannelLayoutPtr input_channel_layout);

  bool ConvertAudio(const scoped_refptr<DecoderBuffer>& input,
                    size_t header_size,
                    size_t max_output_frame_count);

  // On older systems, we don't know how to flush AudioConverter properly, so
  // we fake it by generating a trailing silent buffer.  Returns true if the
  // workaround has been applied.
  bool ApplyEOSWorkaround(const scoped_refptr<DecoderBuffer>& input,
                          AudioBufferList* output_buffers);

  void ResetTimestampState();

  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  AudioDecoderConfig config_;
  std::unique_ptr<ATCodecHelper> codec_helper_;
  AudioStreamBasicDescription output_format_;
  ScopedAudioConverterRef converter_;
  std::deque<scoped_refptr<DecoderBuffer>> queued_input_;
  std::unique_ptr<AudioDiscardHelper> discard_helper_;
  const bool needs_eos_workaround_;
  OutputCB output_cb_;

  DebugBufferLogger debug_buffer_logger_;
  DISALLOW_COPY_AND_ASSIGN(ATAudioDecoder);
};

}  // namespace media

#endif  // PLATFORM_MEDIA_RENDERER_DECODERS_MAC_AT_AUDIO_DECODER_H_
