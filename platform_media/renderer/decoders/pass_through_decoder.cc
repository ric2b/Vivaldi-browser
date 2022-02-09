// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA.

#include "platform_media/renderer/decoders/pass_through_decoder.h"

#include "platform_media/common/platform_logging_util.h"
#include "platform_media/common/platform_media_pipeline_types.h"
#include "platform_media/renderer/decoders/video_frame_transformer.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "media/base/audio_buffer.h"
#include "media/base/audio_decoder.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/decoder_buffer.h"
#include "media/base/demuxer_stream.h"
#include "media/base/video_decoder.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_frame.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace media {

namespace {

// A pass-through decoder is not a real media decoder, because the input and
// output formats are always the same.  Its job is to repackage decoded media
// data from DecoderBuffers into media-type-specific output buffers.
template <DemuxerStream::Type StreamType>
class PassThroughDecoderImpl {
 public:
  using DecoderTraits = DecoderStreamTraits<StreamType>;
  using DecoderConfig = typename DecoderTraits::DecoderConfigType;
  using DecoderType = typename DecoderTraits::DecoderType;
  using DecodeCB = typename DecoderType::DecodeCB;
  using InitCB = typename DecoderType::InitCB;
  using OutputCB = typename DecoderTraits::OutputCB;
  using OutputType = typename DecoderTraits::OutputType;

  PassThroughDecoderImpl() = default;
  ~PassThroughDecoderImpl() = default;

  void Initialize(const DecoderConfig& config,
                  InitCB init_cb,
                  const OutputCB& output_cb) {
    // This can be called multiple times.
    task_runner_ = base::SequencedTaskRunnerHandle::Get();
    DCHECK(config.IsValidConfig());
    DCHECK(config.platform_media_pass_through_);

    config_ = config;
    output_cb_ = output_cb;

    // The caller expects the callback will be called after the call.
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(std::move(init_cb), media::Status()));
  }

  void Decode(scoped_refptr<DecoderBuffer> buffer, DecodeCB decode_cb) {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    DCHECK(config_.IsValidConfig());

    typename media::DecodeStatus status = DecodeStatus::OK;

    if (!buffer->end_of_stream()) {
      scoped_refptr<OutputType> output;
      if (buffer->data_size() > 0) {
        output = DecoderBufferToOutputBuffer(buffer);
      }
      if (output) {
        task_runner_->PostTask(FROM_HERE, base::BindOnce(output_cb_, output));
      } else {
        LOG(WARNING) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
                     << " Detected " << DecoderTraits::ToString()
                     << " DECODE_ERROR";
        status = DecodeStatus::DECODE_ERROR;
      }
    }

    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(std::move(decode_cb), status));
  }

  void Reset(base::OnceClosure closure) {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    task_runner_->PostTask(FROM_HERE, std::move(closure));
  }

 private:
  // This is specialized below for audio and video.
  scoped_refptr<OutputType> DecoderBufferToOutputBuffer(
      const scoped_refptr<DecoderBuffer>& buffer) const;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  DecoderConfig config_;
  OutputCB output_cb_;
};

class PassThroughAudioDecoder : public AudioDecoder {
 public:
  PassThroughAudioDecoder() = default;

  // AudioDecoder implementation.
  void Initialize(const AudioDecoderConfig& config,
                  CdmContext* cdm_context,
                  InitCB init_cb,
                  const OutputCB& output_cb,
                  const WaitingCB& waiting_for_decryption_key_cb) override {
    impl_.Initialize(config, std::move(init_cb), output_cb);
  }

  void Decode(scoped_refptr<DecoderBuffer> buffer,
              DecodeCB decode_cb) override {
    impl_.Decode(buffer, std::move(decode_cb));
  }

  void Reset(base::OnceClosure closure) override {
    impl_.Reset(std::move(closure));
  }

  AudioDecoderType GetDecoderType() const override {
    return AudioDecoderType::kVivPassThrough;
  }

 private:
  PassThroughDecoderImpl<DemuxerStream::AUDIO> impl_;
};

class PassThroughVideoDecoder : public VideoDecoder {
 public:
  PassThroughVideoDecoder() = default;

  // VideoDecoder implementation.
  void Initialize(const VideoDecoderConfig& config,
                  bool low_delay,
                  CdmContext* cdm_context,
                  InitCB init_cb,
                  const OutputCB& output_cb,
                  const WaitingCB& waiting_for_decryption_key_cb) override {
    impl_.Initialize(config, std::move(init_cb), output_cb);
  }

  void Decode(scoped_refptr<DecoderBuffer> buffer,
              DecodeCB decode_cb) override {
    impl_.Decode(buffer, std::move(decode_cb));
  }

  void Reset(base::OnceClosure closure) override {
    impl_.Reset(std::move(closure));
  }
  VideoDecoderType GetDecoderType() const override {
    return VideoDecoderType::kVivPassThrough;
  }

 private:
  PassThroughDecoderImpl<DemuxerStream::VIDEO> impl_;
};

template <>
scoped_refptr<AudioBuffer>
PassThroughDecoderImpl<DemuxerStream::AUDIO>::DecoderBufferToOutputBuffer(
    const scoped_refptr<DecoderBuffer>& buffer) const {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  const int channel_count =
      ChannelLayoutToChannelCount(config_.channel_layout());

  const int channel_size = buffer->data_size() / channel_count;
  const int frame_count = buffer->data_size() / config_.bytes_per_frame();

  typedef const uint8_t* ChannelData;
  std::unique_ptr<ChannelData[]> data(new ChannelData[channel_count]);
  for (int channel = 0; channel < channel_count; ++channel)
    data[channel] = buffer->data() + channel * channel_size;

  VLOG(5) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
          << " samples_per_second : " << config_.samples_per_second();

  return AudioBuffer::CopyFrom(config_.sample_format(),
                               config_.channel_layout(), channel_count,
                               config_.samples_per_second(), frame_count,
                               data.get(), buffer->timestamp());
}

template <>
scoped_refptr<VideoFrame>
PassThroughDecoderImpl<DemuxerStream::VIDEO>::DecoderBufferToOutputBuffer(
    const scoped_refptr<DecoderBuffer>& buffer) const {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  return GetVideoFrameFromMemory(buffer, config_);
}

}  // namespace

template <>
std::unique_ptr<AudioDecoder>
CreatePlatformMediaPassThroughDecoder<DemuxerStream::AUDIO>() {
  return std::make_unique<PassThroughAudioDecoder>();
}

template <>
std::unique_ptr<VideoDecoder>
CreatePlatformMediaPassThroughDecoder<DemuxerStream::VIDEO>() {
  return std::make_unique<PassThroughVideoDecoder>();
}

}  // namespace media
