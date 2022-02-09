// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#include "platform_media/gpu/pipeline/mac/avf_media_reader_runner.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "media/base/bind_to_current_loop.h"

#include "base/vivaldi_switches.h"
#include "platform_media/common/platform_logging_util.h"
#include "platform_media/common/platform_media_pipeline_types.h"
#include "platform_media/gpu/data_source/ipc_data_source.h"
#include "platform_media/gpu/decoders/mac/avf_media_reader.h"
#include "platform_media/gpu/decoders/mac/data_request_handler.h"
#include "platform_media/gpu/pipeline/mac/media_utils_mac.h"

namespace media {

namespace {

void InitializeReader(AVFMediaReader* reader,
                      base::scoped_nsobject<AVAsset> asset,
                      PlatformMediaPipeline::InitializeCB initialize_cb) {
  const bool success = reader->Initialize(std::move(asset));

  auto result = platform_media::mojom::PipelineInitResult::New();
  if (success) {
    result->bitrate = reader->bitrate();
    result->time_info.duration = reader->duration();
    result->time_info.start_time = reader->start_time();

    if (reader->has_audio_track()) {
      result->audio_config.format = SampleFormat::kSampleFormatF32;
      result->audio_config.channel_count =
          reader->audio_stream_format().mChannelsPerFrame;
      result->audio_config.samples_per_second =
          reader->audio_stream_format().mSampleRate;
      VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
              << " Using PlatformAudioConfig : "
              << Loggable(result->audio_config);
    } else {
      VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__ << " No Audio Track ";
    }

    if (reader->has_video_track()) {
      media::Strides strides = reader->GetStrides();
      result->video_config = GetPlatformVideoConfig(
          reader->video_stream_format(), reader->video_transform(),
          strides.stride_Y, strides.stride_UV);
      VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
              << " Using PlatformVideoConfig : "
              << Loggable(result->video_config);
    } else {
      VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__ << " No Video Track ";
    }
    result->success = true;
  }

  std::move(initialize_cb).Run(std::move(result));
}

}  // namespace

AVFMediaReaderRunner::AVFMediaReaderRunner() {
  VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__;
}

AVFMediaReaderRunner::~AVFMediaReaderRunner() {
  VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  data_request_handler_->Stop();

  if (reader_queue_) {
    // Delete the reader on its queue after all pending operations complete.
    __block std::unique_ptr<AVFMediaReader> reader = std::move(reader_);
    dispatch_async(reader_queue_, ^{
      reader.reset();
    });

    dispatch_release(reader_queue_);
  }
}

// static
bool AVFMediaReaderRunner::IsAvailable() {
  static bool initialized = false;
  static bool use_reader = false;
  if (!initialized) {
    initialized = true;
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    std::string s =
        command_line->GetSwitchValueASCII(switches::kVivaldiPlatformMedia);
    if (s == "reader") {
      use_reader = true;
    } else if (s == "player") {
      use_reader = false;
    } else {
      if (!s.empty()) {
        LOG(ERROR) << "Unknown value for --" << switches::kVivaldiPlatformMedia
                   << " - " << s;
      }
      use_reader = true;
    }
    LOG(INFO) << " PROPMEDIA(GPU) : "
              << "use_reader=" << (use_reader ? "yes" : "no");
  }
  return use_reader;
}

void AVFMediaReaderRunner::Initialize(ipc_data_source::Info source_info,
                                      InitializeCB initialize_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
          << " Mime type : " << source_info.mime_type;

  data_request_handler_ = base::MakeRefCounted<media::DataRequestHandler>();
  data_request_handler_->Init(std::move(source_info), nil);

  reader_queue_ = dispatch_queue_create("com.operasoftware.MediaReader",
                                        DISPATCH_QUEUE_SERIAL);
  if (!reader_queue_) {
    LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__
               << ": Failed, could not create dispatch queue";
    std::move(initialize_cb)
        .Run(platform_media::mojom::PipelineInitResult::New());
    return;
  }

  reader_.reset(new AVFMediaReader(reader_queue_));

  // Store things in the block storage so we can move them inside the block.
  __block base::scoped_nsobject<AVAsset> asset(
      data_request_handler_->GetAsset(), base::scoped_policy::RETAIN);
  __block InitializeCB cb = BindToCurrentLoop(std::move(initialize_cb));
  dispatch_async(reader_queue_, ^{
    InitializeReader(reader_.get(), std::move(asset), std::move(cb));
  });
}

void AVFMediaReaderRunner::ReadMediaData(IPCDecodingBuffer buffer) {
  VLOG(5) << " PROPMEDIA(GPU) : " << __FUNCTION__
          << " Renderer asking for media data, stream_type="
          << GetStreamTypeName(buffer.stream_type());
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  __block auto cb = BindToCurrentLoop(base::BindOnce(
      &AVFMediaReaderRunner::DataReady, weak_ptr_factory_.GetWeakPtr()));
  __block IPCDecodingBuffer block_buffer = std::move(buffer);
  dispatch_async(reader_queue_, ^{
    reader_->GetNextMediaSample(block_buffer);
    std::move(cb).Run(std::move(block_buffer));
  });
}

void AVFMediaReaderRunner::WillSeek() {
  VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  data_request_handler_->Suspend();
}

void AVFMediaReaderRunner::Seek(base::TimeDelta time, SeekCB seek_cb) {
  VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  data_request_handler_->Resume();

  __block SeekCB cb = BindToCurrentLoop(std::move(seek_cb));
  dispatch_async(reader_queue_, ^{
    std::move(cb).Run(reader_->Seek(time));
  });
}

void AVFMediaReaderRunner::DataReady(IPCDecodingBuffer ipc_buffer) {
  VLOG(5) << " PROPMEDIA(GPU) : " << __FUNCTION__
          << " stream_type=" << GetStreamTypeName(ipc_buffer.stream_type())
          << " status=" << static_cast<int>(ipc_buffer.status())
          << " suspended_handler=" << data_request_handler_->IsSuspended();
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (ipc_buffer.status() == MediaDataStatus::kMediaError &&
      data_request_handler_->IsSuspended() && ipc_buffer.data_size() > 0) {
    // In |WillSeek()| we interrupt the IPCDataSource, which can easily cause
    // decode errors.  But we do that because of the approaching seek request,
    // so it's fine to ignore the error and repeat the previosly sent data.
    ipc_buffer.set_status(MediaDataStatus::kOk);
  }

  IPCDecodingBuffer::Reply(std::move(ipc_buffer));
}

}  // namespace media
