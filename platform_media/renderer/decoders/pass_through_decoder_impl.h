// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA.

#ifndef PLATFORM_MEDIA_RENDERER_DECODERS_PASS_THROUGH_DECODER_IMPL_H_
#define PLATFORM_MEDIA_RENDERER_DECODERS_PASS_THROUGH_DECODER_IMPL_H_

#include "platform_media/common/feature_toggles.h"

#include "base/single_thread_task_runner.h"
#include "media/base/audio_decoder.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/demuxer_stream.h"
#include "media/base/video_decoder.h"
#include "media/base/video_decoder_config.h"
#include "media/filters/decoder_stream_traits.h"

namespace media {

template <DemuxerStream::Type StreamType>
struct PassThroughDecoderImplTraits : public DecoderStreamTraits<StreamType> {};

template <>
struct PassThroughDecoderImplTraits<DemuxerStream::AUDIO>
    : public DecoderStreamTraits<DemuxerStream::AUDIO> {
  using DecoderConfigType = AudioDecoderConfig;
};

template <>
struct PassThroughDecoderImplTraits<DemuxerStream::VIDEO>
    : public DecoderStreamTraits<DemuxerStream::VIDEO> {
  using DecoderConfigType = VideoDecoderConfig;
};

// A pass-through decoder is not a real media decoder, because the input and
// output formats are always the same.  Its job is to repackage decoded media
// data from DecoderBuffers into media-type-specific output buffers.
template <DemuxerStream::Type StreamType>
class PassThroughDecoderImpl {
 public:
  using DecoderTraits = PassThroughDecoderImplTraits<StreamType>;
  using DecoderConfig = typename DecoderTraits::DecoderConfigType;
  using DecoderType = typename DecoderTraits::DecoderType;
  using DecodeCB = typename DecoderType::DecodeCB;
  using InitCB = typename DecoderType::InitCB;
  using OutputCB = typename DecoderTraits::OutputCB;
  using OutputType = typename DecoderTraits::OutputType;

  explicit PassThroughDecoderImpl(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner);

  void Initialize(const DecoderConfig& config,
                  const InitCB& init_cb,
                  const OutputCB& output_cb);
  void Decode(scoped_refptr<DecoderBuffer> buffer,
              const DecodeCB& decode_cb);
  void Reset(const base::Closure& closure);

 private:
  // Performs decoder config checks specific to the pass-through decoder,
  // beyond the generic DecoderConfig::IsValidConfig() check.
  static bool IsValidConfig(const DecoderConfig& config);

  scoped_refptr<OutputType> DecoderBufferToOutputBuffer(
      const scoped_refptr<DecoderBuffer>& buffer) const;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  DecoderConfig config_;
  OutputCB output_cb_;
};

}  // namespace media

#endif  // PLATFORM_MEDIA_RENDERER_DECODERS_PASS_THROUGH_DECODER_IMPL_H_
