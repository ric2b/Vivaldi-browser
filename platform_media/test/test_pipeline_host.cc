// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2015 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#include "platform_media/test/test_pipeline_host.h"

#include "platform_media/common/platform_ipc_util.h"
#include "platform_media/common/video_frame_transformer.h"
#include "platform_media/gpu/pipeline/platform_media_pipeline.h"
#include "platform_media/renderer/decoders/ipc_demuxer.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/data_buffer.h"
#include "media/base/decoder_buffer.h"
#include "media/base/video_decoder_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

TestPipelineHost::TestPipelineHost() : weak_ptr_factory_(this) {
  for (PlatformStreamType stream_type : AllStreamTypes()) {
    GetElem(ipc_decoding_buffers_, stream_type).Init(stream_type);
  }
}

TestPipelineHost::~TestPipelineHost() = default;

void TestPipelineHost::Initialize(const std::string& mimetype,
                                  InitializeCB callback) {
  CHECK(data_source_);
  CHECK(callback);
  CHECK(!init_cb_);
  init_cb_ = std::move(callback);

  base::MappedReadOnlyRegion region_and_mapping =
      base::ReadOnlySharedMemoryRegion::Create(kIPCSourceSharedMemorySize);
  CHECK(region_and_mapping.IsValid());
  base::ReadOnlySharedMemoryMapping data_source_mapping =
      region_and_mapping.region.Map();
  CHECK(data_source_mapping.IsValid());

  ipc_data_source::Reader source_reader = base::BindRepeating(
      &TestPipelineHost::ReadRawData, weak_ptr_factory_.GetWeakPtr());

  // The reader must be called from the current thread.
  source_reader = BindToCurrentLoop(std::move(source_reader));

  platform_pipeline_ = PlatformMediaPipeline::Create();
  CHECK(platform_pipeline_);
  ipc_data_source::Info source_info;
  source_info.is_streaming = data_source_->IsStreaming();
  if (!data_source_->GetSize(&source_info.size)) {
    source_info.size = -1;
  }
  source_info.mime_type = mimetype;
  source_info.buffer.Init(std::move(data_source_mapping),
                          std::move(source_reader));

  platform_pipeline_->Initialize(
      std::move(source_info),
      base::BindRepeating(&TestPipelineHost::Initialized,
                          weak_ptr_factory_.GetWeakPtr()));
}

void TestPipelineHost::StartWaitingForSeek() {}

void TestPipelineHost::Seek(base::TimeDelta time,
                            PipelineStatusCallback status_cb) {
  platform_pipeline_->Seek(
      time, base::BindRepeating(&TestPipelineHost::SeekDone,
                                base::Passed(std::move(status_cb))));
}

void TestPipelineHost::ReadDecodedData(PlatformStreamType stream_type,
                                       DemuxerStream::ReadCB read_cb) {
  CHECK(GetElem(ipc_decoding_buffers_, stream_type))
      << "Overlapping reads are not supported";

  GetElem(ipc_decoding_buffers_, stream_type)
      .set_reply_cb(base::AdaptCallbackForRepeating(
          base::BindOnce(&TestPipelineHost::DataReady,
                         weak_ptr_factory_.GetWeakPtr(), std::move(read_cb))));
  platform_pipeline_->ReadMediaData(
      std::move(GetElem(ipc_decoding_buffers_, stream_type)));
}

void TestPipelineHost::SeekDone(PipelineStatusCallback status_cb,
                                bool success) {
  if (success) {
    std::move(status_cb).Run(PipelineStatus::PIPELINE_OK);
    return;
  }

  LOG(ERROR) << " PROPMEDIA(TEST) : " << __FUNCTION__
             << ": PIPELINE_ERROR_ABORT";
  std::move(status_cb).Run(PipelineStatus::PIPELINE_ERROR_ABORT);
}

void TestPipelineHost::Initialized(bool success,
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

void TestPipelineHost::ReadRawData(ipc_data_source::Buffer buffer) {
  CHECK(buffer);
  int size = buffer.GetRequestedSize();
  CHECK(size > 0);

  // As we use a weak_ptr, this instance holding raw_data_mapping_ can be
  // deleted before data_source finishes accessing the data. To ensure that the
  // pointer stays valid move the mapping as a bound callback argument and move
  // it back when the callback is called.
  //
  // C++ does not define the order of argument evaluation, so get the pointer
  // before base::Bind() can be evaluated and call the move constructor for
  // raw_data_mapping_.
  uint8_t* data = raw_data_mapping_.GetMemoryAs<uint8_t>();
  int64_t position = buffer.GetReadPosition();
  data_source_->Read(
      position, size, data,
      base::AdaptCallbackForRepeating(base::BindOnce(
          &TestPipelineHost::OnReadRawDataDone, weak_ptr_factory_.GetWeakPtr(),
          std::move(buffer), std::move(raw_data_mapping_))));
}

void TestPipelineHost::OnReadRawDataDone(
    ipc_data_source::Buffer buffer,
    base::WritableSharedMemoryMapping raw_data_mapping,
    int size) {
  raw_data_mapping_ = std::move(raw_data_mapping);
  buffer.SetReadSize(size);
  ipc_data_source::Buffer::SendReply(std::move(buffer));
}

void TestPipelineHost::DataReady(DemuxerStream::ReadCB read_cb,
                                 IPCDecodingBuffer ipc_buffer) {
  PlatformStreamType stream_type = ipc_buffer.stream_type();
  CHECK(ipc_buffer);
  CHECK(!GetElem(ipc_decoding_buffers_, stream_type)) << "unexpected reply";

  DemuxerStream::Status reply_status = DemuxerStream::kOk;
  scoped_refptr<DecoderBuffer> decoder_buffer;
  switch (ipc_buffer.status()) {
    case MediaDataStatus::kOk:
      decoder_buffer = DecoderBuffer::CopyFrom(ipc_buffer.GetDataForTests(),
                                               ipc_buffer.data_size());
      decoder_buffer->set_timestamp(ipc_buffer.timestamp());
      decoder_buffer->set_duration(ipc_buffer.duration());
      break;
    case MediaDataStatus::kEOS:
      decoder_buffer = DecoderBuffer::CreateEOSBuffer();
      break;
    case MediaDataStatus::kConfigChanged:
      reply_status = DemuxerStream::kConfigChanged;
      decoder_buffer = nullptr;
      switch (stream_type) {
        case PlatformStreamType::kAudio:
          audio_config_ = ipc_buffer.GetAudioConfig();
          break;
        case PlatformStreamType::kVideo:
          video_config_ = ipc_buffer.GetVideoConfig();
          break;
      }
      break;
    case MediaDataStatus::kMediaError:
      decoder_buffer = new DecoderBuffer(0);
      break;
  }
  GetElem(ipc_decoding_buffers_, stream_type) = std::move(ipc_buffer);

  std::move(read_cb).Run(reply_status, decoder_buffer);
}

}  // namespace media
