// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (C) 2015 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#include "platform_media/gpu/test/test_pipeline_host.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "platform_media/gpu/pipeline/platform_media_pipeline_create.h"
#include "platform_media/gpu/pipeline/platform_media_pipeline.h"
#include "media/base/data_buffer.h"
#include "media/base/decoder_buffer.h"
#include "platform_media/renderer/decoders/ipc_demuxer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

TestPipelineHost::DataSourceAdapter::~DataSourceAdapter() = default;

void TestPipelineHost::DataSourceAdapter::Read(int64_t position,
                                               int size,
                                               uint8_t* data,
                                               const ReadCB& read_cb) {
  data_source_->Read(position, size, data, read_cb);
}

void TestPipelineHost::DataSourceAdapter::Stop() {
  data_source_->Stop();
}

void TestPipelineHost::DataSourceAdapter::Abort() {
  data_source_->Abort();
}

bool TestPipelineHost::DataSourceAdapter::GetSize(int64_t* size_out) {
  return data_source_->GetSize(size_out);
}

bool TestPipelineHost::DataSourceAdapter::IsStreaming() {
  return data_source_->IsStreaming();
}

void TestPipelineHost::DataSourceAdapter::SetBitrate(int bitrate) {
  data_source_->SetBitrate(bitrate);
}

TestPipelineHost::TestPipelineHost(media::DataSource* data_source)
    : data_source_adapter_(data_source),
      platform_pipeline_(content::PlatformMediaPipelineCreate(
          &data_source_adapter_,
          base::Bind(&TestPipelineHost::OnAudioConfigChanged,
                     base::Unretained(this)),
          base::Bind(&TestPipelineHost::OnVideoConfigChanged,
                     base::Unretained(this)),
          media::PlatformMediaDecodingMode::SOFTWARE,
          base::Callback<bool(void)>())) {}

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
                            const media::PipelineStatusCB& status_cb) {
  platform_pipeline_->Seek(time,
                           base::Bind(&TestPipelineHost::SeekDone, status_cb));
}

void TestPipelineHost::Stop() {
  platform_pipeline_.reset();
}

void TestPipelineHost::ReadDecodedData(
    media::PlatformMediaDataType type,
    const media::DemuxerStream::ReadCB& read_cb) {
  CHECK(read_cb_[type].is_null()) << "Overlapping reads are not supported";

  read_cb_[type] = read_cb;

  switch (type) {
    case media::PLATFORM_MEDIA_AUDIO:
      platform_pipeline_->ReadAudioData(base::Bind(
          &TestPipelineHost::DataReady, base::Unretained(this), type));
      break;

    case media::PLATFORM_MEDIA_VIDEO: {
      const uint32_t dummy_texture_id = 0;
      platform_pipeline_->ReadVideoData(
          base::Bind(&TestPipelineHost::DataReady, base::Unretained(this),
                     type),
          dummy_texture_id);
      break;
    }

    default:
      FAIL();
      break;
  }
}

media::PlatformAudioConfig TestPipelineHost::audio_config() const {
  return audio_config_;
}
media::PlatformVideoConfig TestPipelineHost::video_config() const {
  return video_config_;
}

void TestPipelineHost::SeekDone(const media::PipelineStatusCB& status_cb,
                                bool success) {
  status_cb.Run(success ? media::PIPELINE_OK : media::PIPELINE_ERROR_ABORT);
}

void TestPipelineHost::Initialized(
    bool success,
    int bitrate,
    const media::PlatformMediaTimeInfo& time_info,
    const media::PlatformAudioConfig& audio_config,
    const media::PlatformVideoConfig& video_config) {
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
    media::PlatformMediaDataType type,
    const scoped_refptr<media::DataBuffer>& buffer) {
  CHECK(!read_cb_[type].is_null());

  scoped_refptr<media::DecoderBuffer> decoder_buffer =
      new media::DecoderBuffer(0);
  if (buffer) {
    if (buffer->end_of_stream()) {
      decoder_buffer = media::DecoderBuffer::CreateEOSBuffer();
    } else {
      decoder_buffer =
          media::DecoderBuffer::CopyFrom(buffer->data(), buffer->data_size());
      decoder_buffer->set_timestamp(buffer->timestamp());
      decoder_buffer->set_duration(buffer->duration());
    }
  }

  base::ResetAndReturn(&read_cb_[type])
      .Run(media::DemuxerStream::kOk, decoder_buffer);
}

void TestPipelineHost::OnAudioConfigChanged(
    const media::PlatformAudioConfig& audio_config) {
  CHECK(!read_cb_[media::PLATFORM_MEDIA_AUDIO].is_null());
  audio_config_ = audio_config;
  base::ResetAndReturn(&read_cb_[media::PLATFORM_MEDIA_AUDIO])
      .Run(media::DemuxerStream::kConfigChanged, nullptr);
}

void TestPipelineHost::OnVideoConfigChanged(
    const media::PlatformVideoConfig& video_config) {
  CHECK(!read_cb_[media::PLATFORM_MEDIA_VIDEO].is_null());
  video_config_ = video_config;
  base::ResetAndReturn(&read_cb_[media::PLATFORM_MEDIA_VIDEO])
      .Run(media::DemuxerStream::kConfigChanged, nullptr);
}

}  // namespace content
