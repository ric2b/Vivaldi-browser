// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#include "media/filters/ipc_demuxer_stream.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_util.h"
#include "media/filters/ipc_media_pipeline_host.h"
#include "media/filters/platform_media_pipeline_types.h"

namespace media {

namespace {

PlatformMediaDataType DemuxerTypeToPlatformMediaDataType(
    DemuxerStream::Type type) {
  switch (type) {
    case DemuxerStream::AUDIO:
      return PLATFORM_MEDIA_AUDIO;

    case DemuxerStream::VIDEO:
      return PLATFORM_MEDIA_VIDEO;

    default:
      NOTREACHED();
      return PlatformMediaDataType();
  }
}

}  // namespace

IPCDemuxerStream::IPCDemuxerStream(
    Type type,
    IPCMediaPipelineHost* ipc_media_pipeline_host)
    : type_(type),
      ipc_media_pipeline_host_(ipc_media_pipeline_host),
      weak_ptr_factory_(this) {
  DCHECK(ipc_media_pipeline_host_);
}

IPCDemuxerStream::~IPCDemuxerStream() {
}

void IPCDemuxerStream::Read(const ReadCB& read_cb) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(read_cb_.is_null()) << "Overlapping reads are not supported";

  // Don't accept any additional reads if we've been told to stop.
  if (ipc_media_pipeline_host_ == NULL) {
    read_cb.Run(kOk, DecoderBuffer::CreateEOSBuffer());
    return;
  }

  read_cb_ = read_cb;

  ipc_media_pipeline_host_->ReadDecodedData(
      DemuxerTypeToPlatformMediaDataType(type_),
      base::Bind(&IPCDemuxerStream::DataReady, weak_ptr_factory_.GetWeakPtr()));
}

AudioDecoderConfig IPCDemuxerStream::audio_decoder_config() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(type_, AUDIO);

  PlatformAudioConfig platform_audio_config =
      ipc_media_pipeline_host_->audio_config();
  DCHECK(platform_audio_config.is_valid());

  // This demuxer stream is different from "normal" demuxers in that it outputs
  // decoded data.  To fit into existing media pipeline we hard code some
  // information, which is normally read from the data stream.
  AudioDecoderConfig audio_config;
  audio_config.Initialize(
      kCodecPCM, platform_audio_config.format,
      GuessChannelLayout(platform_audio_config.channel_count),
      platform_audio_config.samples_per_second, EmptyExtraData(), Unencrypted(),
      base::TimeDelta(), 0);

  return audio_config;
}

VideoDecoderConfig IPCDemuxerStream::video_decoder_config() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(type_, VIDEO);

  PlatformVideoConfig platform_video_config =
      ipc_media_pipeline_host_->video_config();
  DCHECK(platform_video_config.is_valid());

  // This demuxer stream is different from "normal" demuxers in that it outputs
  // decoded data.  To fit into existing media pipeline we hard code some
  // information, which is normally read from the data stream.
  VideoDecoderConfig video_config;
  video_config.Initialize(
      kCodecH264, H264PROFILE_MAIN, PIXEL_FORMAT_YV12, COLOR_SPACE_UNSPECIFIED,
      platform_video_config.coded_size, platform_video_config.visible_rect,
      platform_video_config.natural_size,
      std::vector<uint8_t>(
          reinterpret_cast<const uint8_t*>(&platform_video_config.planes),
          reinterpret_cast<const uint8_t*>(&platform_video_config.planes)
              + sizeof(platform_video_config.planes)),
      Unencrypted());

  return video_config;
}

VideoRotation IPCDemuxerStream::video_rotation() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(type_, VIDEO);
  return ipc_media_pipeline_host_->video_config().rotation;
}

DemuxerStream::Type IPCDemuxerStream::type() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return type_;
}

void IPCDemuxerStream::EnableBitstreamConverter() {
  DCHECK(thread_checker_.CalledOnValidThread());
  /* Intentionally empty */
}

bool IPCDemuxerStream::SupportsConfigChanges() {
  DCHECK(thread_checker_.CalledOnValidThread());
#if defined(OS_WIN)
  return true;
#else
  return false;
#endif
}

void IPCDemuxerStream::Stop() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!read_cb_.is_null()) {
    base::ResetAndReturn(&read_cb_)
        .Run(DemuxerStream::kOk, DecoderBuffer::CreateEOSBuffer());
  }

  ipc_media_pipeline_host_ = NULL;
}

void IPCDemuxerStream::DataReady(Status status,
                                 const scoped_refptr<DecoderBuffer>& buffer) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!read_cb_.is_null())
    base::ResetAndReturn(&read_cb_).Run(status, buffer);
}

}  // namespace media
