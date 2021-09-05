// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2015 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#include "platform_media/test/test_pipeline_host.h"

#include "platform_media/gpu/pipeline/platform_media_pipeline.h"
#include "platform_media/gpu/pipeline/platform_media_pipeline_factory.h"
#include "platform_media/renderer/decoders/ipc_demuxer.h"
#include "platform_media/common/video_frame_transformer.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/data_buffer.h"
#include "media/base/decoder_buffer.h"
#include "media/base/video_decoder_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

TestPipelineHost::TestPipelineHost() : weak_ptr_factory_(this) {
  for (int i = 0; i < kPlatformMediaDataTypeCount; ++i) {
    ipc_decoding_buffers_[i].Init(static_cast<PlatformMediaDataType>(i));
  }
}

TestPipelineHost::~TestPipelineHost() = default;

void TestPipelineHost::Initialize(const std::string& mimetype,
                                  InitializeCB callback) {
  CHECK(data_source_);
  CHECK(callback);
  CHECK(!init_cb_);
  init_cb_ = std::move(callback);

  if (!platform_pipeline_factory_) {
    platform_pipeline_factory_ = PlatformMediaPipelineFactory::Create();
  }
  platform_pipeline_ = platform_pipeline_factory_->CreatePipeline();
  CHECK(platform_pipeline_);
  ipc_data_source::Reader source_reader = base::BindRepeating(
      &TestPipelineHost::ReadRawData, weak_ptr_factory_.GetWeakPtr());

  // The reader must be called from the current thread
  source_reader = BindToCurrentLoop(std::move(source_reader));
  ipc_data_source::Info source_info;
  source_info.is_streaming = data_source_->IsStreaming();
  if (!data_source_->GetSize(&source_info.size)) {
    source_info.size = -1;
  }
  source_info.mime_type = mimetype;
  platform_pipeline_->Initialize(std::move(source_reader),
                                 std::move(source_info),
                                 base::Bind(&TestPipelineHost::Initialized,
                                            weak_ptr_factory_.GetWeakPtr()));
}

void TestPipelineHost::StartWaitingForSeek() {
}

void TestPipelineHost::Seek(base::TimeDelta time,
                            PipelineStatusCallback status_cb) {
  platform_pipeline_->Seek(time,
                           base::Bind(&TestPipelineHost::SeekDone,
                                      base::Passed(std::move(status_cb))));
}

void TestPipelineHost::ReadDecodedData(
    PlatformMediaDataType type,
    DemuxerStream::ReadCB read_cb) {
  CHECK(ipc_decoding_buffers_[type]) << "Overlapping reads are not supported";

  ipc_decoding_buffers_[type].set_reply_cb(base::AdaptCallbackForRepeating(
      base::BindOnce(&TestPipelineHost::DataReady,
                     weak_ptr_factory_.GetWeakPtr(), std::move(read_cb))));
  platform_pipeline_->ReadMediaData(std::move(ipc_decoding_buffers_[type]));
}

void TestPipelineHost::SeekDone(PipelineStatusCallback status_cb,
                                bool success) {
  if(success) {
    std::move(status_cb).Run(PipelineStatus::PIPELINE_OK);
    return;
  }

  LOG(ERROR) << " PROPMEDIA(TEST) : " << __FUNCTION__
             << ": PIPELINE_ERROR_ABORT";
  std::move(status_cb).Run(PipelineStatus::PIPELINE_ERROR_ABORT);
}

void TestPipelineHost::Initialized(
    bool success,
    int bitrate,
    const PlatformMediaTimeInfo& time_info,
    const PlatformAudioConfig& audio_config,
    const PlatformVideoConfig& video_config) {
  CHECK(init_cb_);

  if (audio_config.is_valid()) {
    audio_config_ = audio_config;
  }

  if (video_config.is_valid()) {
    video_config_ = video_config;
  }

  success = success && bitrate >= 0;
  bitrate_ = bitrate;
  time_info_ = time_info;

  std::move(init_cb_).Run(success);
}

void TestPipelineHost::ReadRawData(int64_t position,
                                   int size,
                                   ipc_data_source::ReadCB read_cb) {
  CHECK(size > 0);
  std::unique_ptr<uint8_t[]> data_holder(new uint8_t[size]);

  // C++ does not define the order of argument evaluation, so get the pointer
  // before base::Bind() can be evaluated and call the move constructor for
  // data_holder.
  uint8_t* data = data_holder.get();
  data_source_->Read(
      position, size, data,
      base::AdaptCallbackForRepeating(base::BindOnce(
          &TestPipelineHost::OnReadRawDataDone, weak_ptr_factory_.GetWeakPtr(),
          std::move(read_cb), std::move(data_holder))));
}

void TestPipelineHost::OnReadRawDataDone(ipc_data_source::ReadCB read_cb, std::unique_ptr<uint8_t[]> data,
                                         int size) {
  std::move(read_cb).Run(size, data.get());
}

void TestPipelineHost::DataReady(DemuxerStream::ReadCB read_cb,
    IPCDecodingBuffer ipc_buffer) {

  PlatformMediaDataType type = ipc_buffer.type();
  CHECK(ipc_buffer);
  CHECK(!ipc_decoding_buffers_[type]) << "unexpected reply";

  DemuxerStream::Status reply_status = DemuxerStream::kOk;
  scoped_refptr<DecoderBuffer> decoder_buffer;
  switch (ipc_buffer.status()) {
    case MediaDataStatus::kOk:
      decoder_buffer =
          DecoderBuffer::CopyFrom(ipc_buffer.GetDataForTests(), ipc_buffer.data_size());
      decoder_buffer->set_timestamp(ipc_buffer.timestamp());
      decoder_buffer->set_duration(ipc_buffer.duration());
      break;
    case MediaDataStatus::kEOS:
      decoder_buffer = DecoderBuffer::CreateEOSBuffer();
      break;
    case MediaDataStatus::kConfigChanged:
      reply_status = DemuxerStream::kConfigChanged;
      decoder_buffer = nullptr;
      switch (type) {
        case PlatformMediaDataType::PLATFORM_MEDIA_AUDIO:
          audio_config_ = ipc_buffer.GetAudioConfig();
          break;
        case PlatformMediaDataType::PLATFORM_MEDIA_VIDEO:
          video_config_ = ipc_buffer.GetVideoConfig();
          break;
      }
      break;
    case MediaDataStatus::kMediaError:
      decoder_buffer = new DecoderBuffer(0);
      break;
  }
  ipc_decoding_buffers_[type] = std::move(ipc_buffer);

  std::move(read_cb).Run(reply_status, decoder_buffer);
}

}  // namespace media
