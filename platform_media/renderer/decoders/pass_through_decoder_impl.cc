// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA.

#include "platform_media/renderer/decoders/pass_through_decoder_impl.h"

#include "platform_media/common/platform_logging_util.h"
#include "platform_media/common/platform_media_pipeline_types.h"
#include "platform_media/common/video_frame_transformer.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/synchronization/waitable_event.h"
#include "media/base/audio_buffer.h"
#include "media/base/decoder_buffer.h"
#include "media/base/video_frame.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace media {

template <DemuxerStream::Type StreamType>
PassThroughDecoderImpl<StreamType>::PassThroughDecoderImpl(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner)
    : task_runner_(task_runner) {
}

template <DemuxerStream::Type StreamType>
void PassThroughDecoderImpl<StreamType>::Initialize(
    const DecoderConfig& config,
    const InitCB& init_cb,
    const OutputCB& output_cb) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(config.IsValidConfig());

  if (!IsValidConfig(config)) {
    VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
            << " Media Config not accepted for codec : "
            << GetCodecName(config.codec());
    task_runner_->PostTask(FROM_HERE, base::Bind(init_cb, false));
    return;
  } else {
    VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
            << " Supported decoder config for codec : "
            << Loggable(config);
  }

  config_ = config;
  output_cb_ = output_cb;

  task_runner_->PostTask(FROM_HERE, base::Bind(init_cb, true));
}

template <DemuxerStream::Type StreamType>
void PassThroughDecoderImpl<StreamType>::Decode(
    scoped_refptr<DecoderBuffer> buffer,
    const DecodeCB& decode_cb) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(IsValidConfig(config_));

  typename media::DecodeStatus status = DecodeStatus::OK;

  if (!buffer->end_of_stream()) {
    scoped_refptr<OutputType> output;
    if (buffer->data_size() > 0)
      output = DecoderBufferToOutputBuffer(buffer);
    else
      LOG(WARNING) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
                   << " Detected " << DecoderTraits::ToString()
                   << " decoding error";

    if (output) {
      task_runner_->PostTask(FROM_HERE, base::Bind(output_cb_, output));
    } else {
      LOG(WARNING) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
                   << " Detected " << DecoderTraits::ToString()
                   << " DECODE_ERROR";
      status = DecodeStatus::DECODE_ERROR;
    }
  }

  task_runner_->PostTask(FROM_HERE, base::Bind(decode_cb, status));
}

template <DemuxerStream::Type StreamType>
void PassThroughDecoderImpl<StreamType>::Reset(const base::Closure& closure) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  task_runner_->PostTask(FROM_HERE, closure);
}

template <>
bool PassThroughDecoderImpl<DemuxerStream::AUDIO>::IsValidConfig(
    const DecoderConfig& config) {

  LOG_IF(WARNING, !(config.codec() == AudioCodec::kCodecPCM))
      << " PROPMEDIA(RENDERER) : " << __FUNCTION__
      << " Unsupported Audio codec : " << GetCodecName(config.codec());

  if (config.codec() == AudioCodec::kCodecPCM) {
    LOG_IF(WARNING, !(ChannelLayoutToChannelCount(config.channel_layout()) > 0))
        << " PROPMEDIA(RENDERER) : " << __FUNCTION__
        << " Channel count is zero for : " << GetCodecName(config.codec());
    LOG_IF(WARNING, !(config.bytes_per_frame() > 0))
        << " PROPMEDIA(RENDERER) : " << __FUNCTION__
        << " Bytes per frame is zero for : " << GetCodecName(config.codec());
  }

  return config.codec() == AudioCodec::kCodecPCM &&
         ChannelLayoutToChannelCount(config.channel_layout()) > 0 &&
         config.bytes_per_frame() > 0;
}

template <>
bool PassThroughDecoderImpl<DemuxerStream::VIDEO>::IsValidConfig(
    const DecoderConfig& config) {

  LOG_IF(WARNING, !(config.codec() == VideoCodec::kCodecH264))
      << " PROPMEDIA(RENDERER) : " << __FUNCTION__
      << " Unsupported Video codec : " << GetCodecName(config.codec());

  if (config.codec() == VideoCodec::kCodecH264) {
    LOG_IF(WARNING, !(config.extra_data().size() == sizeof(PlatformVideoConfig().planes)))
        << " PROPMEDIA(RENDERER) : " << __FUNCTION__
        << " Extra data has wrong size : " << config.extra_data().size()
        << " Expected size : " << sizeof(PlatformVideoConfig().planes)
        << Loggable(config);

    LOG_IF(WARNING, !(config.profile() >= VideoCodecProfile::H264PROFILE_MIN))
        << " PROPMEDIA(RENDERER) : " << __FUNCTION__
        << " Unsupported Video profile (too low) ? : "
        << GetProfileName(config.profile())
        << " Minimum is " << GetProfileName(VideoCodecProfile::H264PROFILE_MIN);

    LOG_IF(WARNING, !(config.profile() <= VideoCodecProfile::H264PROFILE_MAX))
        << " PROPMEDIA(RENDERER) : " << __FUNCTION__
        << " Unsupported Video profile (too high) ? : "
        << GetProfileName(config.profile())
        << " Maximum is " << GetProfileName(VideoCodecProfile::H264PROFILE_MAX);
  }

  return config.codec() == VideoCodec::kCodecH264 &&
         config.extra_data().size() == sizeof(PlatformVideoConfig().planes);
}

template <>
scoped_refptr<AudioBuffer>
PassThroughDecoderImpl<DemuxerStream::AUDIO>::DecoderBufferToOutputBuffer(
    const scoped_refptr<DecoderBuffer>& buffer) const {
  DCHECK(task_runner_->BelongsToCurrentThread());

  const int channel_count =
      ChannelLayoutToChannelCount(config_.channel_layout());

  const int channel_size = buffer->data_size() / channel_count;
  const int frame_count = buffer->data_size() / config_.bytes_per_frame();

  typedef const uint8_t* ChannelData;
  std::unique_ptr<ChannelData[]> data(new ChannelData[channel_count]);
  for (int channel = 0; channel < channel_count; ++channel)
    data[channel] = buffer->data() + channel * channel_size;

  VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
          << " samples_per_second : " << config_.samples_per_second();

  return AudioBuffer::CopyFrom(config_.sample_format(),
                               config_.channel_layout(),
                               channel_count,
                               config_.samples_per_second(),
                               frame_count,
                               data.get(),
                               buffer->timestamp());
}

template <>
scoped_refptr<VideoFrame>
PassThroughDecoderImpl<DemuxerStream::VIDEO>::DecoderBufferToOutputBuffer(
    const scoped_refptr<DecoderBuffer>& buffer) const {
  DCHECK(task_runner_->BelongsToCurrentThread());

  return GetVideoFrameFromMemory(buffer, config_);
}

template class PassThroughDecoderImpl<DemuxerStream::AUDIO>;
template class PassThroughDecoderImpl<DemuxerStream::VIDEO>;

}  // namespace media
