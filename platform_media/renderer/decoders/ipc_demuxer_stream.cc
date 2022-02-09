// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#include "platform_media/renderer/decoders/ipc_demuxer_stream.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_util.h"
#include "platform_media/common/platform_logging_util.h"
#include "platform_media/common/platform_media_pipeline_types.h"
#include "platform_media/renderer/pipeline/ipc_media_pipeline_host.h"

namespace media {

namespace {

PlatformStreamType DemuxerTypeToPlatformStreamType(DemuxerStream::Type type) {
  switch (type) {
    case DemuxerStream::AUDIO:
      return PlatformStreamType::kAudio;
    case DemuxerStream::VIDEO:
      return PlatformStreamType::kVideo;
    default:
      NOTREACHED();
      return PlatformStreamType::kAudio;
  }
}

}  // namespace

IPCDemuxerStream::IPCDemuxerStream(
    Type type,
    IPCMediaPipelineHost* ipc_media_pipeline_host)
    : type_(type),
      ipc_media_pipeline_host_(ipc_media_pipeline_host),
      is_enabled_(true),
      weak_ptr_factory_(this) {
  DCHECK(ipc_media_pipeline_host_);
}

IPCDemuxerStream::~IPCDemuxerStream() {}

void IPCDemuxerStream::Read(ReadCB read_cb) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(read_cb_.is_null()) << "Overlapping reads are not supported";

  // Don't accept any additional reads if we've been told to stop.
  if (!ipc_media_pipeline_host_) {
    std::move(read_cb).Run(kOk, DecoderBuffer::CreateEOSBuffer());
    return;
  }

  if (!is_enabled_) {
    VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
            << " Read from disabled stream, returning EOS";
    // Callback can be unset, see VB-51064.
    if (!read_cb_.is_null()) {
      std::move(read_cb_).Run(kOk, DecoderBuffer::CreateEOSBuffer());
    }
    return;
  }

  read_cb_ = std::move(read_cb);

  ipc_media_pipeline_host_->ReadDecodedData(
      DemuxerTypeToPlatformStreamType(type_),
      base::BindOnce(&IPCDemuxerStream::DataReady,
                     weak_ptr_factory_.GetWeakPtr()));
}

bool IPCDemuxerStream::enabled() const {
  return is_enabled_;
}

void IPCDemuxerStream::set_enabled(bool enabled, base::TimeDelta timestamp) {
  if (enabled == is_enabled_)
    return;

  is_enabled_ = enabled;
  if (!is_enabled_ && !read_cb_.is_null()) {
    VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
            << " Read from disabled stream, returning EOS";
    std::move(read_cb_).Run(kOk, DecoderBuffer::CreateEOSBuffer());
    return;
  }
}

AudioDecoderConfig IPCDemuxerStream::audio_decoder_config() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(type_, AUDIO);

  PlatformAudioConfig platform_audio_config =
      ipc_media_pipeline_host_->audio_config();
  DCHECK(platform_audio_config.is_valid());

  VLOG(5) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
          << " PlatformAudioConfig from IPCMediaPipelineHost : "
          << Loggable(platform_audio_config);

  // This demuxer stream is different from "normal" demuxers in that it outputs
  // decoded data.  To fit into existing media pipeline we hard code some
  // information, which is normally read from the data stream.
  AudioDecoderConfig audio_config;
  audio_config.Initialize(
      AudioCodec::kPCM, platform_audio_config.format,
      GuessChannelLayout(platform_audio_config.channel_count),
      platform_audio_config.samples_per_second, EmptyExtraData(),
      EncryptionScheme::kUnencrypted, base::TimeDelta(), 0);
  audio_config.platform_media_pass_through_ = true;

  VLOG(5) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
          << " Created AudioDecoderConfig with partially HARDCODED values :"
          << Loggable(audio_config);

  return audio_config;
}

VideoDecoderConfig IPCDemuxerStream::video_decoder_config() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(type_, VIDEO);

  PlatformVideoConfig platform_video_config =
      ipc_media_pipeline_host_->video_config();
  DCHECK(platform_video_config.is_valid());

  VLOG(5) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
          << " Creating VideoDecoderConfig : VideoCodec::kH264 with "
             "HARDCODED values";
  // This demuxer stream is different from "normal" demuxers in that it outputs
  // decoded data.  To fit into existing media pipeline we hard code some
  // information, which is normally read from the data stream.
  VideoDecoderConfig video_config;
  video_config.Initialize(
      VideoCodec::kH264, VideoCodecProfile::H264PROFILE_MAIN,
      VideoDecoderConfig::AlphaMode::kIsOpaque,
      VideoColorSpace(VideoColorSpace::PrimaryID::UNSPECIFIED,
                      VideoColorSpace::TransferID::UNSPECIFIED,
                      VideoColorSpace::MatrixID::UNSPECIFIED,
                      gfx::ColorSpace::RangeID::DERIVED),
      platform_video_config.rotation, platform_video_config.coded_size,
      platform_video_config.visible_rect, platform_video_config.natural_size,
      std::vector<uint8_t>(
          reinterpret_cast<const uint8_t*>(&platform_video_config.planes),
          reinterpret_cast<const uint8_t*>(&platform_video_config.planes) +
              sizeof(platform_video_config.planes)),
      EncryptionScheme::kUnencrypted);
  video_config.platform_media_pass_through_ = true;

  VLOG(5) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
          << " VideoCodecProfile : " << GetProfileName(video_config.profile());

  return video_config;
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
    std::move(read_cb_).Run(DemuxerStream::kOk,
                            DecoderBuffer::CreateEOSBuffer());
  }

  ipc_media_pipeline_host_ = NULL;
}

void IPCDemuxerStream::DataReady(Status status,
                                 scoped_refptr<DecoderBuffer> buffer) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (read_cb_) {
    std::move(read_cb_).Run(status, buffer);
  }
}

}  // namespace media
