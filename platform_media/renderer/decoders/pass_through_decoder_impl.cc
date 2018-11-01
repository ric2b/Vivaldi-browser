// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA.

#include "platform_media/renderer/decoders/pass_through_decoder_impl.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/synchronization/waitable_event.h"
#include "media/base/audio_buffer.h"
#include "media/base/decoder_buffer.h"
#include "media/base/video_frame.h"
#include "platform_media/common/platform_media_pipeline_types.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace media {

namespace {

// This function is used as observer of frame of
// VideoFrame::WrapExternalYuvData to make sure we keep reference to
// DecoderBuffer object as long as we need it.
void BufferHolder(const scoped_refptr<DecoderBuffer>& buffer) {
  /* Intentionally empty */
}

const PlatformVideoConfig::Plane* GetPlanes(const VideoDecoderConfig& config) {
  DCHECK(!config.extra_data().empty());
  return reinterpret_cast<const PlatformVideoConfig::Plane*>(
      &config.extra_data().front());
}

scoped_refptr<VideoFrame> GetVideoFrameFromMemory(
    const scoped_refptr<DecoderBuffer>& buffer,
    const VideoDecoderConfig& config) {
  const PlatformVideoConfig::Plane* planes = GetPlanes(config);

  for (size_t i = 0; i < VideoFrame::NumPlanes(PIXEL_FORMAT_YV12); ++i) {
    if (planes[i].offset + planes[i].size > int(buffer->data_size())) {
      DLOG(ERROR) << "Buffer doesn't match video format";
      return nullptr;
    }
  }

  // We need to ensure that our data buffer stays valid long enough.
  // For this reason we pass it as an argument to |no_longer_needed_cb|, this
  // way, thanks to base::Bind, we keep reference to the buffer.
  scoped_refptr<VideoFrame> frame = VideoFrame::WrapExternalYuvData(
      config.format(), config.coded_size(), config.visible_rect(),
      config.natural_size(), planes[VideoFrame::kYPlane].stride,
      planes[VideoFrame::kUPlane].stride, planes[VideoFrame::kVPlane].stride,
      const_cast<uint8_t*>(buffer->data() + planes[VideoFrame::kYPlane].offset),
      const_cast<uint8_t*>(buffer->data() + planes[VideoFrame::kUPlane].offset),
      const_cast<uint8_t*>(buffer->data() + planes[VideoFrame::kVPlane].offset),
      buffer->timestamp());
  frame->AddDestructionObserver(base::Bind(&BufferHolder, buffer));
  return frame;
}

scoped_refptr<VideoFrame> GetVideoFrameFromTexture(
    const scoped_refptr<DecoderBuffer>& buffer,
    const VideoDecoderConfig& config,
    std::unique_ptr<media::PassThroughDecoderTexture> texture) {
  DCHECK(texture);
   gpu::MailboxHolder mailbox_holders[media::VideoFrame::kMaxPlanes] = {
       *texture->mailbox_holder};
  return VideoFrame::WrapNativeTextures(PIXEL_FORMAT_ARGB,
      mailbox_holders, texture->mailbox_holder_release_cb,
      config.coded_size(), config.visible_rect(), config.natural_size(),
      buffer->timestamp());
}

}  // namespace

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
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(config.IsValidConfig());

  if (!IsValidConfig(config)) {
    task_runner_->PostTask(FROM_HERE, base::Bind(init_cb, false));
    return;
  }

  config_ = config;
  output_cb_ = output_cb;

  task_runner_->PostTask(FROM_HERE, base::Bind(init_cb, true));
}

template <DemuxerStream::Type StreamType>
void PassThroughDecoderImpl<StreamType>::Decode(
    const scoped_refptr<DecoderBuffer>& buffer,
    const DecodeCB& decode_cb) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(IsValidConfig(config_));

  typename media::DecodeStatus status = media::DecodeStatus::OK;

  if (!buffer->end_of_stream()) {
    scoped_refptr<OutputType> output;
    if (buffer->data_size() > 0)
      output = DecoderBufferToOutputBuffer(buffer);
    else
      DVLOG(1) << "Detected " << DecoderTraits::ToString() << " decoding error";

    if (output)
      task_runner_->PostTask(FROM_HERE, base::Bind(output_cb_, output));
    else
      status = media::DecodeStatus::DECODE_ERROR;
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
  return config.codec() == kCodecPCM &&
         ChannelLayoutToChannelCount(config.channel_layout()) > 0 &&
         config.bytes_per_frame() > 0;
}

template <>
bool PassThroughDecoderImpl<DemuxerStream::VIDEO>::IsValidConfig(
    const DecoderConfig& config) {
  return config.codec() == kCodecH264 &&
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

  std::unique_ptr<AutoReleasedPassThroughDecoderTexture> wrapped_texture =
      buffer->PassWrappedTexture();
  if (wrapped_texture)
    return GetVideoFrameFromTexture(buffer, config_, wrapped_texture->Pass());

  return GetVideoFrameFromMemory(buffer, config_);
}

template class PassThroughDecoderImpl<DemuxerStream::AUDIO>;
template class PassThroughDecoderImpl<DemuxerStream::VIDEO>;

}  // namespace media
