// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA.

#ifndef MEDIA_FILTERS_PASS_THROUGH_AUDIO_DECODER_H_
#define MEDIA_FILTERS_PASS_THROUGH_AUDIO_DECODER_H_

#include "media/base/audio_decoder.h"
#include "media/filters/pass_through_decoder_impl.h"

namespace media {

class MEDIA_EXPORT PassThroughAudioDecoder : public AudioDecoder {
 public:
  explicit PassThroughAudioDecoder(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner);
  ~PassThroughAudioDecoder() override;

  // AudioDecoder implementation.
  void Initialize(const AudioDecoderConfig& config,
                  const SetCdmReadyCB& set_cdm_ready_cb,
                  const InitCB& init_cb,
                  const OutputCB& output_cb) override;
  void Decode(const scoped_refptr<DecoderBuffer>& buffer,
              const DecodeCB& decode_cb) override;
  void Reset(const base::Closure& closure) override;
  std::string GetDisplayName() const override;

 private:
  PassThroughDecoderImpl<DemuxerStream::AUDIO> impl_;
};

}  // namespace media

#endif  // MEDIA_FILTERS_PASS_THROUGH_AUDIO_DECODER_H_
