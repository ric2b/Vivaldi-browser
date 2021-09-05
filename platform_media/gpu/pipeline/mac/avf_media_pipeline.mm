// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#include "platform_media/gpu/pipeline/mac/avf_media_pipeline.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "platform_media/gpu/decoders/mac/avf_data_buffer_queue.h"
#include "platform_media/gpu/decoders/mac/avf_media_decoder.h"
#include "media/base/data_buffer.h"
#include "media/base/video_frame.h"
#include "platform_media/common/platform_logging_util.h"
#include "platform_media/common/platform_media_pipeline_types.h"
#include "platform_media/common/mac/platform_media_pipeline_types_mac.h"

namespace media {

class AVFMediaPipeline::MediaDecoderClient : public AVFMediaDecoderClient {
 public:
  explicit MediaDecoderClient(AVFMediaPipeline* avf_media_pipeline)
      : avf_media_pipeline_(avf_media_pipeline) {
    DCHECK(avf_media_pipeline_ != NULL);
  }

  void StreamHasEnded() override {
    for (auto& queue : avf_media_pipeline_->media_queues_) {
      if (queue) {
        queue->SetEndOfStream();
      }
    }
  }

  bool HasAvailableCapacity() override {
    if (IsMemoryLimitReached())
      return false;

    for (auto& queue : avf_media_pipeline_->media_queues_) {
      if (queue && queue->HasAvailableCapacity())
        return true;
    }
    return false;
  }

  void MediaSamplesReady(
      PlatformMediaDataType type, scoped_refptr<DataBuffer> buffer) override {
    DCHECK(buffer);
    AVFDataBufferQueue* queue = avf_media_pipeline_->media_queues_[type].get();
    DCHECK(queue);

    queue->BufferReady(buffer);

    if (IsMemoryLimitReached())
      avf_media_pipeline_->DataBufferCapacityDepleted();
  }

 private:
  bool IsMemoryLimitReached() {
    // Maximum memory usage allowed for the whole pipeline.  Choosing the same
    // value FFmpegDemuxer is using, for lack of a better heuristic.
    const size_t kPipelineMemoryLimit = 150 * 1024 * 1024;

    size_t memory_left = kPipelineMemoryLimit;
    for (auto& queue : avf_media_pipeline_->media_queues_) {
      if (!queue)
        continue;

      if (queue->memory_usage() > memory_left) {
        LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__
                   << " Memory limit reached";
        return true;
      }
      memory_left -= queue->memory_usage();
    }

    return false;
  }

  AVFMediaPipeline* const avf_media_pipeline_;
};

AVFMediaPipeline::AVFMediaPipeline()
    : weak_ptr_factory_(this) {
  VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__;
}

AVFMediaPipeline::~AVFMediaPipeline() {
  DCHECK(thread_checker_.CalledOnValidThread());
  VLOG(3) << " PROPMEDIA(GPU) : " << __FUNCTION__
          << " ~AVFMediaPipeline()";
}

void AVFMediaPipeline::Initialize( ipc_data_source::Info source_info,
                                  InitializeCB initialize_cb) {
  DCHECK(thread_checker_.CalledOnValidThread());

  TRACE_EVENT_ASYNC_BEGIN0("IPC_MEDIA", "AVFMediaPipeline::Initialize", this);
  media_decoder_client_.reset(new MediaDecoderClient(this));
  media_decoder_.reset(new AVFMediaDecoder(media_decoder_client_.get()));
  media_decoder_->Initialize(
      std::move(source_info),
      base::Bind(&AVFMediaPipeline::MediaDecoderInitialized,
                 weak_ptr_factory_.GetWeakPtr(), std::move(initialize_cb)));
}

void AVFMediaPipeline::ReadMediaData(IPCDecodingBuffer buffer) {
  PlatformMediaDataType type = buffer.type();
  VLOG(5) << " PROPMEDIA(GPU) : " << __FUNCTION__
          << " Renderer asking for media data, type=" << type;
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(media_queues_[type]);

  media_queues_[type]->Read(std::move(buffer));
}

void AVFMediaPipeline::Seek(base::TimeDelta time, SeekCB seek_cb) {
  DCHECK(thread_checker_.CalledOnValidThread());

  media_decoder_->Seek(time,
                       base::Bind(&AVFMediaPipeline::SeekDone,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  std::move(seek_cb)));
}

void AVFMediaPipeline::SeekDone(SeekCB seek_cb, bool success) {
  DCHECK(thread_checker_.CalledOnValidThread());

  for (auto& queue : media_queues_) {
    if (queue) {
      queue->Flush();
    }
  }

  std::move(seek_cb).Run(success);
}

void AVFMediaPipeline::MediaDecoderInitialized(
    const InitializeCB& initialize_cb,
    bool success) {
  DCHECK(thread_checker_.CalledOnValidThread());

  TRACE_EVENT_ASYNC_END0("IPC_MEDIA", "AVFMediaPipeline::Initialize", this);

  if (!success) {
    LOG(WARNING) << " PROPMEDIA(GPU) : " << __FUNCTION__
                 << " Failed (" << success << ")";
    initialize_cb.Run(false,
                      -1,
                      media::PlatformMediaTimeInfo(),
                      media::PlatformAudioConfig(),
                      media::PlatformVideoConfig());
    return;
  }

  const base::Closure capacity_available_cb =
      base::Bind(&AVFMediaPipeline::DataBufferCapacityAvailable,
                 weak_ptr_factory_.GetWeakPtr());
  const base::Closure capacity_depleted_cb =
      base::Bind(&AVFMediaPipeline::DataBufferCapacityDepleted,
                 weak_ptr_factory_.GetWeakPtr());

  if (media_decoder_->has_video_track()) {
    PlatformMediaDataType type = PlatformMediaDataType::PLATFORM_MEDIA_VIDEO;
    // >=3 frames for fps <= 25
    base::TimeDelta capacity = base::TimeDelta::FromMilliseconds(120);
    media_queues_[type] = std::make_unique<AVFDataBufferQueue>(
        type, capacity, capacity_available_cb, capacity_depleted_cb);
  }
  if (media_decoder_->has_audio_track()) {
    PlatformMediaDataType type = PlatformMediaDataType::PLATFORM_MEDIA_AUDIO;
    // AVFMediaDecoder decodes audio ahead of video.
    base::TimeDelta capacity = base::TimeDelta::FromMilliseconds(200);
    media_queues_[type] = std::make_unique<AVFDataBufferQueue>(
        type, capacity, capacity_available_cb, capacity_depleted_cb);
  }

  media::PlatformAudioConfig audio_config;
  if (media_decoder_->has_audio_track()) {
    audio_config.format = SampleFormat::kSampleFormatPlanarF32;
    audio_config.channel_count =
        media_decoder_->audio_stream_format().mChannelsPerFrame;
    audio_config.samples_per_second =
        media_decoder_->audio_stream_format().mSampleRate;
    VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
            << " Using PlatformAudioConfig : "
            << Loggable(audio_config);
  }

  media::PlatformVideoConfig video_config;
  if (media_decoder_->has_video_track()) {
    video_config = GetPlatformVideoConfig(
        media_decoder_->video_stream_format(),
        media_decoder_->video_transform());
    VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
            << " Using PlatformVideoConfig : "
            << Loggable(video_config);
  }

  media::PlatformMediaTimeInfo time_info;
  time_info.duration = media_decoder_->duration();
  time_info.start_time = media_decoder_->start_time();

  initialize_cb.Run(true, media_decoder_->bitrate(), time_info, audio_config,
                    video_config);
}

void AVFMediaPipeline::DataBufferCapacityAvailable() {
  DCHECK(thread_checker_.CalledOnValidThread());

  media_decoder_->NotifyStreamCapacityAvailable();
}

void AVFMediaPipeline::DataBufferCapacityDepleted() {
  DCHECK(thread_checker_.CalledOnValidThread());

  media_decoder_->NotifyStreamCapacityDepleted();
}

}  // namespace media
