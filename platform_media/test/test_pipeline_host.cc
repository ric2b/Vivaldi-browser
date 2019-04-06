// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2015 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#include "platform_media/test/test_pipeline_host.h"

#include "platform_media/gpu/pipeline/platform_media_pipeline.h"
#include "platform_media/gpu/pipeline/platform_media_pipeline_create.h"
#include "platform_media/renderer/decoders/ipc_demuxer.h"
#include "platform_media/common/video_frame_transformer.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "media/base/data_buffer.h"
#include "media/base/decoder_buffer.h"
#include "media/base/video_decoder_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

TestPipelineHost::TestPipelineHost(DataSource* data_source)
    : data_source_adapter_(data_source),
      platform_pipeline_(PlatformMediaPipelineCreate(
          &data_source_adapter_,
          base::Bind(&TestPipelineHost::OnAudioConfigChanged,
                     base::Unretained(this)),
          base::Bind(&TestPipelineHost::OnVideoConfigChanged,
                     base::Unretained(this)))) {}

TestPipelineHost::~TestPipelineHost() = default;

void TestPipelineHost::Initialize(const std::string& mimetype,
                                  const InitializeCB& callback) {
  CHECK(init_cb_.is_null());
  init_cb_ = callback;

  platform_pipeline_->Initialize(
      mimetype,
      base::Bind(&TestPipelineHost::Initialized, base::Unretained(this)));
}

void TestPipelineHost::StartWaitingForSeek() {
}

void TestPipelineHost::Seek(base::TimeDelta time,
                            const PipelineStatusCB& status_cb) {
  platform_pipeline_->Seek(time,
                           base::Bind(&TestPipelineHost::SeekDone, status_cb));
}

void TestPipelineHost::Stop() {
  platform_pipeline_.reset();
}

void TestPipelineHost::ReadDecodedData(
    PlatformMediaDataType type,
    const DemuxerStream::ReadCB& read_cb) {
  CHECK(read_cb_[type].is_null()) << "Overlapping reads are not supported";

  read_cb_[type] = read_cb;

  switch (type) {
    case PlatformMediaDataType::PLATFORM_MEDIA_AUDIO:
      platform_pipeline_->ReadAudioData(base::Bind(
          &TestPipelineHost::DataReady, base::Unretained(this), type));
      break;

    case PlatformMediaDataType::PLATFORM_MEDIA_VIDEO: {
      platform_pipeline_->ReadVideoData(
          base::Bind(&TestPipelineHost::DataReady, base::Unretained(this),
                     type));
      break;
    }

    default:
      FAIL();
      break;
  }
}

void TestPipelineHost::AppendBuffer(const scoped_refptr<DecoderBuffer>& buffer,
                                      const VideoDecoder::DecodeCB& decode_cb) {
  data_source_adapter_.AppendBuffer(buffer, decode_cb);
}

bool TestPipelineHost::DecodeVideo(const VideoDecoderConfig& config,
                                   const DecodeVideoCB& read_cb) {
  LOG(INFO) << " PROPMEDIA(RENDERER) : " << __FUNCTION__;
  config_ = config;
  decoded_video_frame_callback_ = read_cb;
  ReadDecodedData(PlatformMediaDataType::PLATFORM_MEDIA_VIDEO,
                  base::Bind(&TestPipelineHost::DecodedVideoReady,
                             base::Unretained(this)));
  return true;
}

void TestPipelineHost::DecodedVideoReady(
        DemuxerStream::Status status,
        scoped_refptr<DecoderBuffer> buffer) {
  LOG(INFO) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
            << " status (" << status << ") : " << buffer->AsHumanReadableString();
  if (status == DemuxerStream::kOk) {
    scoped_refptr<VideoFrame> frame = GetVideoFrameFromMemory(buffer, config_);
    decoded_video_frame_callback_.Run(DemuxerStream::Status::kOk, frame);
  } else {
    decoded_video_frame_callback_.Run(status, scoped_refptr<VideoFrame>());
  }
}

bool TestPipelineHost::HasEnoughData() {
  return true;
}

int TestPipelineHost::GetMaxDecodeBuffers() {
  return 0;
}

PlatformAudioConfig TestPipelineHost::audio_config() const {
  return audio_config_;
}
PlatformVideoConfig TestPipelineHost::video_config() const {
  return video_config_;
}

void TestPipelineHost::SeekDone(const PipelineStatusCB& status_cb,
                                bool success) {
  if(success) {
    status_cb.Run(PipelineStatus::PIPELINE_OK);
    return;
  }

  LOG(ERROR) << " PROPMEDIA(TEST) : " << __FUNCTION__
             << ": PIPELINE_ERROR_ABORT";
  status_cb.Run(PipelineStatus::PIPELINE_ERROR_ABORT);
}

void TestPipelineHost::Initialized(
    bool success,
    int bitrate,
    const PlatformMediaTimeInfo& time_info,
    const PlatformAudioConfig& audio_config,
    const PlatformVideoConfig& video_config) {
  CHECK(!init_cb_.is_null());

  if (audio_config.is_valid()) {
    audio_config_ = audio_config;
  }

  if (video_config.is_valid()) {
    video_config_ = video_config;
  }

  success = success && bitrate >= 0;

  base::ResetAndReturn(&init_cb_)
      .Run(success, bitrate, time_info, audio_config, video_config);
}

void TestPipelineHost::DataReady(
    PlatformMediaDataType type,
    const scoped_refptr<DataBuffer>& buffer) {
  CHECK(!read_cb_[type].is_null());

  scoped_refptr<DecoderBuffer> decoder_buffer =
      new DecoderBuffer(0);
  if (buffer) {
    if (buffer->end_of_stream()) {
      decoder_buffer = DecoderBuffer::CreateEOSBuffer();
    } else {
      decoder_buffer =
          DecoderBuffer::CopyFrom(buffer->data(), buffer->data_size());
      decoder_buffer->set_timestamp(buffer->timestamp());
      decoder_buffer->set_duration(buffer->duration());
    }
  }

  base::ResetAndReturn(&read_cb_[type])
      .Run(DemuxerStream::kOk, decoder_buffer);
}

void TestPipelineHost::OnAudioConfigChanged(
    const PlatformAudioConfig& audio_config) {
  CHECK(!read_cb_[PlatformMediaDataType::PLATFORM_MEDIA_AUDIO].is_null());
  audio_config_ = audio_config;
  base::ResetAndReturn(&read_cb_[PlatformMediaDataType::PLATFORM_MEDIA_AUDIO])
      .Run(DemuxerStream::kConfigChanged, nullptr);
}

void TestPipelineHost::OnVideoConfigChanged(
    const PlatformVideoConfig& video_config) {
  CHECK(!read_cb_[PlatformMediaDataType::PLATFORM_MEDIA_VIDEO].is_null());
  video_config_ = video_config;
  base::ResetAndReturn(&read_cb_[PlatformMediaDataType::PLATFORM_MEDIA_VIDEO])
      .Run(DemuxerStream::kConfigChanged, nullptr);
}

}  // namespace media
