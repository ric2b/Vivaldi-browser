// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#ifndef PLATFORM_MEDIA_RENDERER_DECODERS_PASS_THROUGH_VIDEO_DECODER_H_
#define PLATFORM_MEDIA_RENDERER_DECODERS_PASS_THROUGH_VIDEO_DECODER_H_

#include "platform_media/common/feature_toggles.h"

#include "media/base/video_decoder.h"
#include "platform_media/renderer/decoders/pass_through_decoder_impl.h"

namespace media {

class MEDIA_EXPORT PassThroughVideoDecoder : public VideoDecoder {
 public:
  explicit PassThroughVideoDecoder(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner);
  ~PassThroughVideoDecoder() override;

  // VideoDecoder implementation.
  void Initialize(const VideoDecoderConfig& config,
                  bool low_delay,
                  CdmContext* cdm_context,
                  const InitCB& init_cb,
                  const OutputCB& output_cb,
                  const WaitingForDecryptionKeyCB& waiting_for_decryption_key_cb) override;
  void Decode(scoped_refptr<DecoderBuffer> buffer,
              const DecodeCB& decode_cb) override;
  void Reset(const base::Closure& closure) override;
  std::string GetDisplayName() const override;

 private:
  PassThroughDecoderImpl<DemuxerStream::VIDEO> impl_;
};

}  // namespace media

#endif  // PLATFORM_MEDIA_RENDERER_DECODERS_PASS_THROUGH_VIDEO_DECODER_H_
