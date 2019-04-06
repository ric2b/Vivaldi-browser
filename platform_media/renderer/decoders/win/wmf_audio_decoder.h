// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2015 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#ifndef PLATFORM_MEDIA_RENDERER_DECODERS_WIN_WMF_AUDIO_DECODER_H_
#define PLATFORM_MEDIA_RENDERER_DECODERS_WIN_WMF_AUDIO_DECODER_H_

#include "platform_media/common/feature_toggles.h"

#include <string>

#include "media/base/audio_decoder.h"
#include "platform_media/renderer/decoders/win/wmf_decoder_impl.h"

namespace media {

// Decodes AAC audio streams using Windows Media Foundation library.
class MEDIA_EXPORT WMFAudioDecoder : public AudioDecoder {
 public:
  WMFAudioDecoder(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner);
  ~WMFAudioDecoder() override;

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
  WMFDecoderImpl<DemuxerStream::AUDIO> impl_;

  DISALLOW_COPY_AND_ASSIGN(WMFAudioDecoder);
};

}  // namespace media

#endif  // PLATFORM_MEDIA_RENDERER_DECODERS_WIN_WMF_AUDIO_DECODER_H_
