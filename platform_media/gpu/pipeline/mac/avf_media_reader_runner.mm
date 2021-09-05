// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#include "platform_media/gpu/pipeline/mac/avf_media_reader_runner.h"

#include "base/command_line.h"
#include "base/mac/mac_util.h"
#include "media/base/bind_to_current_loop.h"

#include "base/vivaldi_switches.h"
#include "platform_media/common/platform_logging_util.h"
#include "platform_media/common/platform_media_pipeline_types.h"
#include "platform_media/common/mac/platform_media_pipeline_types_mac.h"
#include "platform_media/gpu/decoders/mac/avf_media_reader.h"
#include "platform_media/gpu/data_source/ipc_data_source.h"
#include "platform_media/gpu/decoders/mac/data_source_loader.h"

namespace media {

namespace {

void InitializeReader(AVFMediaReader* reader,
                      base::scoped_nsobject<AVAsset> asset,
                      PlatformMediaPipeline::InitializeCB initialize_cb) {
  const bool success = reader->Initialize(std::move(asset));

  int bitrate = -1;
  media::PlatformMediaTimeInfo time_info;
  media::PlatformAudioConfig audio_config;
  media::PlatformVideoConfig video_config;

  if (success) {
    bitrate = reader->bitrate();
    time_info.duration = reader->duration();
    time_info.start_time = reader->start_time();

    if (reader->has_audio_track()) {
      audio_config.format = SampleFormat::kSampleFormatF32;
      audio_config.channel_count =
          reader->audio_stream_format().mChannelsPerFrame;
      audio_config.samples_per_second =
          reader->audio_stream_format().mSampleRate;
      VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
              << " Using PlatformAudioConfig : "
              << Loggable(audio_config);
    } else {
      VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
              << " No Audio Track ";
    }

    if (reader->has_video_track()) {
      video_config = GetPlatformVideoConfig(
          reader->video_stream_format(), reader->video_transform());
      VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
              << " Using PlatformVideoConfig : "
              << Loggable(video_config);
    } else {
      VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
              << " No Video Track ";
    }
  }

  std::move(initialize_cb)
      .Run(success, bitrate, time_info, audio_config, video_config);
}

}  // namespace

AVFMediaReaderRunner::AVFMediaReaderRunner()
    : reader_queue_(nullptr), will_seek_(false), weak_ptr_factory_(this) {
  VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__;
}

AVFMediaReaderRunner::~AVFMediaReaderRunner() {
  VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());

  [data_source_loader_ stop];

  if (reader_queue_) {
    // Because |reader_queue_| is a FIFO queue, this effectively "joins" with
    // the queue: Any |reader_| tasks currently in flight will run on
    // |reader_queue_|, and then the following block will run.
    dispatch_sync(reader_queue_, ^{
      reader_.reset();
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
      // Use the older player-based implementation on 10.15 due to VB-58686.
      use_reader = !base::mac::IsOS10_15();
    }
    LOG(INFO) << " PROPMEDIA(GPU) : "
              << "use_reader=" << (use_reader ? "yes" : "no");
  }
  return use_reader;
}

// static
void AVFMediaReaderRunner::WarmUp() {
  if (!IsAvailable())
    return;
  VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__;
  ipc_data_source::Info source_info;
  source_info.mime_type = "video/mp4";
  base::scoped_nsobject<DataSourceLoader> data_source_loader;

  // Normally AVFMediaReader runs on own queue and data_source_loader lives on
  // the main thread queue. But the warmup function runs on the main thread and
  // should not return until the AVFMediaReader::Initialize returns. Yet the
  // data source should live on a queue that never blocks to avoid a deadlock.
  // So we reverse queues and directly run Initialize on the main thread.
  dispatch_queue_t loader_queue = dispatch_queue_create(
      "com.vivaldi.warmupMediaLoader", DISPATCH_QUEUE_SERIAL);

  // To warmup the sandbox we do not need to read anything. So we pass an empty
  // source read callback that leads to errors on any read attempt.
  data_source_loader.reset([[DataSourceLoader alloc]
      initWithDataSource:ipc_data_source::Reader()
          withSourceInfo:std::move(source_info)
       withDispatchQueue:loader_queue]);

  AVFMediaReader reader(dispatch_get_main_queue());
  base::scoped_nsobject<AVAsset> asset([data_source_loader asset],
                                       base::scoped_policy::RETAIN);
  reader.Initialize(std::move(asset));
  dispatch_async(loader_queue, ^{
    [data_source_loader stop];
  });
  dispatch_release(loader_queue);
}

void AVFMediaReaderRunner::Initialize(ipc_data_source::Reader source_reader,
                                      ipc_data_source::Info source_info,
                                      InitializeCB initialize_cb) {
  DCHECK(thread_checker_.CalledOnValidThread());

  VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
          << " Mime type : " << source_info.mime_type;

  data_source_loader_.reset([[DataSourceLoader alloc]
      initWithDataSource:std::move(source_reader)
          withSourceInfo:std::move(source_info)
       withDispatchQueue:dispatch_get_main_queue()]);

  reader_queue_ = dispatch_queue_create("com.operasoftware.MediaReader",
                                        DISPATCH_QUEUE_SERIAL);
  if (!reader_queue_) {
    LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__
               << ": Failed, could not create dispatch queue";
    initialize_cb.Run(false,
                      -1,
                      media::PlatformMediaTimeInfo(),
                      media::PlatformAudioConfig(),
                      media::PlatformVideoConfig());
    return;
  }

  reader_.reset(new AVFMediaReader(reader_queue_));

  // Store things in the block storage so we can move them inside the block.
  __block base::scoped_nsobject<AVAsset> asset([data_source_loader_ asset],
                                               base::scoped_policy::RETAIN);
  __block InitializeCB cb = BindToCurrentLoop(std::move(initialize_cb));
  dispatch_async(reader_queue_, ^{
    InitializeReader(reader_.get(), std::move(asset), std::move(cb));
  });
}

void AVFMediaReaderRunner::ReadMediaData(IPCDecodingBuffer buffer) {
  PlatformMediaDataType type = buffer.type();
  VLOG(5) << " PROPMEDIA(GPU) : " << __FUNCTION__
          << " Renderer asking for media data, type=" << type;
  DCHECK(thread_checker_.CalledOnValidThread());

  __block auto cb = BindToCurrentLoop(base::BindOnce(
      &AVFMediaReaderRunner::DataReady, weak_ptr_factory_.GetWeakPtr()));
  __block IPCDecodingBuffer block_buffer = std::move(buffer);
  dispatch_async(reader_queue_, ^{
    reader_->GetNextMediaSample(block_buffer.type(), &block_buffer);
    std::move(cb).Run(std::move(block_buffer));
  });
}

void AVFMediaReaderRunner::WillSeek() {
  VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());

  will_seek_ = true;
  [data_source_loader_.get() suspend];
}

void AVFMediaReaderRunner::Seek(base::TimeDelta time, SeekCB seek_cb) {
  VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());

  [data_source_loader_.get() resume];

  will_seek_ = false;

  auto cb = BindToCurrentLoop(std::move(seek_cb));
  dispatch_async(reader_queue_, ^{
    std::move(cb).Run(reader_->Seek(time));
  });
}

void AVFMediaReaderRunner::DataReady(IPCDecodingBuffer ipc_buffer) {
  PlatformMediaDataType type = ipc_buffer.type();
  VLOG(5) << " PROPMEDIA(GPU) : " << __FUNCTION__ << " type=" << type
          << " status=" << static_cast<int>(ipc_buffer.status())
          << " will_seek=" << will_seek_;
  DCHECK(thread_checker_.CalledOnValidThread());

  if (ipc_buffer.status() == MediaDataStatus::kMediaError && will_seek_ &&
      ipc_buffer.data_size() > 0) {
    // In |WillSeek()| we interrupt the IPCDataSource, which can easily cause
    // decode errors.  But we do that because of the approaching seek request,
    // so it's fine to ignore the error and repeat the previosly sent data.
    ipc_buffer.set_status(MediaDataStatus::kOk);
  }

  IPCDecodingBuffer::Reply(std::move(ipc_buffer));
}

}  // namespace media
