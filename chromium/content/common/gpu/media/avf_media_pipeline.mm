// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#include "content/common/gpu/media/avf_media_pipeline.h"

#include "base/bind.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "content/common/gpu/media/avf_data_buffer_queue.h"
#include "content/common/gpu/media/avf_media_decoder.h"
#include "media/base/data_buffer.h"
#include "media/base/video_frame.h"
#include "media/filters/platform_media_pipeline_types.h"
#include "media/filters/platform_media_pipeline_types_mac.h"

namespace content {

class AVFMediaPipeline::MediaDecoderClient : public AVFMediaDecoderClient {
 public:
  explicit MediaDecoderClient(AVFMediaPipeline* avf_media_pipeline)
      : avf_media_pipeline_(avf_media_pipeline) {
    DCHECK(avf_media_pipeline_ != NULL);
  }

  void AudioSamplesReady(
      const scoped_refptr<media::DataBuffer>& buffer) override {
    HandleNewBuffer(buffer, avf_media_pipeline_->audio_queue_.get());
  }
  void VideoFrameReady(
      const scoped_refptr<media::DataBuffer>& buffer) override {
    HandleNewBuffer(buffer, avf_media_pipeline_->video_queue_.get());
  }

  void StreamHasEnded() override {
    if (avf_media_pipeline_->audio_queue_.get() != NULL)
      avf_media_pipeline_->audio_queue_->SetEndOfStream();

    if (avf_media_pipeline_->video_queue_.get() != NULL)
      avf_media_pipeline_->video_queue_->SetEndOfStream();
  }

  bool HasAvailableCapacity() override {
    if (IsMemoryLimitReached())
      return false;

    if (avf_media_pipeline_->audio_queue_.get() != NULL &&
        avf_media_pipeline_->audio_queue_->HasAvailableCapacity())
      return true;

    if (avf_media_pipeline_->video_queue_.get() != NULL &&
        avf_media_pipeline_->video_queue_->HasAvailableCapacity())
      return true;

    return false;
  }

 private:
  void HandleNewBuffer(const scoped_refptr<media::DataBuffer>& buffer,
                       AVFDataBufferQueue* queue) {
    DCHECK(buffer.get() != NULL);
    DCHECK(queue != NULL);

    queue->BufferReady(buffer);

    if (IsMemoryLimitReached())
      avf_media_pipeline_->DataBufferCapacityDepleted();
  }

  bool IsMemoryLimitReached() {
    // Maximum memory usage allowed for the whole pipeline.  Choosing the same
    // value FFmpegDemuxer is using, for lack of a better heuristic.
    const size_t kPipelineMemoryLimit = 150 * 1024 * 1024;

    size_t memory_left = kPipelineMemoryLimit;

    AVFDataBufferQueue* const queues[] = {
        avf_media_pipeline_->audio_queue_.get(),
        avf_media_pipeline_->video_queue_.get(),
    };

    for (size_t i = 0; i < arraysize(queues); ++i) {
      if (queues[i] == NULL)
        continue;

      if (queues[i]->memory_usage() > memory_left) {
        DVLOG(1) << "Memory limit reached";
        return true;
      }

      memory_left -= queues[i]->memory_usage();
    }

    return false;
  }

  AVFMediaPipeline* const avf_media_pipeline_;
};

AVFMediaPipeline::AVFMediaPipeline(IPCDataSource* data_source)
    : data_source_(data_source), weak_ptr_factory_(this) {
  DCHECK(data_source_ != NULL);
}

AVFMediaPipeline::~AVFMediaPipeline() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(3) << "~AVFMediaPipeline()";
}

void AVFMediaPipeline::Initialize(const std::string& mime_type,
                                  const InitializeCB& initialize_cb) {
  DCHECK(thread_checker_.CalledOnValidThread());

  TRACE_EVENT_ASYNC_BEGIN0("IPC_MEDIA", "AVFMediaPipeline::Initialize", this);
  media_decoder_client_.reset(new MediaDecoderClient(this));
  media_decoder_.reset(new AVFMediaDecoder(media_decoder_client_.get()));
  media_decoder_->Initialize(
      data_source_,
      mime_type,
      base::Bind(&AVFMediaPipeline::MediaDecoderInitialized,
                 weak_ptr_factory_.GetWeakPtr(),
                 initialize_cb));
}

void AVFMediaPipeline::ReadAudioData(const ReadDataCB& read_audio_data_cb) {
  DVLOG(5) << "Renderer asking for audio data";
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(audio_queue_.get() != NULL);

  audio_queue_->Read(base::Bind(&AVFMediaPipeline::AudioBufferReady,
                                weak_ptr_factory_.GetWeakPtr(),
                                read_audio_data_cb));
}

void AVFMediaPipeline::ReadVideoData(const ReadDataCB& read_video_data_cb,
                                     uint32_t /* texture_id */) {
  DVLOG(5) << "Renderer asking for video data";
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(video_queue_.get() != NULL);

  video_queue_->Read(base::Bind(&AVFMediaPipeline::VideoBufferReady,
                                weak_ptr_factory_.GetWeakPtr(),
                                read_video_data_cb));
}

void AVFMediaPipeline::Seek(base::TimeDelta time, const SeekCB& seek_cb) {
  DCHECK(thread_checker_.CalledOnValidThread());

  media_decoder_->Seek(time,
                       base::Bind(&AVFMediaPipeline::SeekDone,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  seek_cb));
}

void AVFMediaPipeline::SeekDone(const SeekCB& seek_cb, bool success) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (audio_queue_.get() != NULL)
    audio_queue_->Flush();
  if (video_queue_.get() != NULL)
    video_queue_->Flush();

  seek_cb.Run(success);
}

void AVFMediaPipeline::AudioBufferReady(
    const ReadDataCB& read_audio_data_cb,
    const scoped_refptr<media::DataBuffer>& buffer) {
  DVLOG(5) << "Ready to reply to renderer with decoded audio";
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(buffer.get() != NULL);

  read_audio_data_cb.Run(buffer);
}

void AVFMediaPipeline::VideoBufferReady(
    const ReadDataCB& read_video_data_cb,
    const scoped_refptr<media::DataBuffer>& buffer) {
  DVLOG(5) << "Ready to reply to renderer with decoded video";
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(buffer.get() != NULL);

  read_video_data_cb.Run(buffer);
}

void AVFMediaPipeline::MediaDecoderInitialized(
    const InitializeCB& initialize_cb,
    bool success) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(3) << "MediaDecoderInitialized(" << success << ")";

  TRACE_EVENT_ASYNC_END0("IPC_MEDIA", "AVFMediaPipeline::Initialize", this);

  if (!success) {
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
    video_queue_.reset(
        new AVFDataBufferQueue(AVFDataBufferQueue::VIDEO,
                               // >=3 frames for fps <= 25
                               base::TimeDelta::FromMilliseconds(120),
                               capacity_available_cb, capacity_depleted_cb));
  }
  if (media_decoder_->has_audio_track()) {
    audio_queue_.reset(
        new AVFDataBufferQueue(AVFDataBufferQueue::AUDIO,
                               // AVFMediaDecoder decodes audio ahead of video.
                               base::TimeDelta::FromMilliseconds(200),
                               capacity_available_cb, capacity_depleted_cb));
  }

  media::PlatformAudioConfig audio_config;
  if (media_decoder_->has_audio_track()) {
    audio_config.format = media::kSampleFormatPlanarF32;
    audio_config.channel_count =
        media_decoder_->audio_stream_format().mChannelsPerFrame;
    audio_config.samples_per_second =
        media_decoder_->audio_stream_format().mSampleRate;
  }

  media::PlatformVideoConfig video_config;
  if (media_decoder_->has_video_track()) {
    video_config = media::GetPlatformVideoConfig(
        media_decoder_->video_stream_format(),
        media_decoder_->video_transform());
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

}  // namespace content
