// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#include "platform_media/renderer/pipeline/ipc_media_pipeline_host_impl.h"

#include <algorithm>
#include <map>
#include <queue>

#include "base/callback_helpers.h"
#include "base/numerics/safe_conversions.h"
#include "base/trace_event/trace_event.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/data_source.h"
#include "media/base/decoder_buffer.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "platform_media/common/feature_toggles.h"
#include "platform_media/common/media_pipeline_messages.h"
#include "platform_media/common/platform_logging_util.h"
#include "platform_media/common/platform_media_pipeline_types.h"
#include "platform_media/common/video_frame_transformer.h"

namespace media {

namespace {

const char* const kDecodedDataReadTraceEventNames[] = {"ReadAudioData",
                                                       "ReadVideoData"};
static_assert(arraysize(kDecodedDataReadTraceEventNames) ==
                  PlatformMediaDataType::PLATFORM_MEDIA_DATA_TYPE_COUNT,
              "Incorrect number of defined tracing event names.");

template <PlatformMediaDataType type, typename ConfigType>
void HandleConfigChange(const ConfigType& new_config,
                        ConfigType* current_config,
                        MediaPipelineMsg_DecodedDataReady_Params* params) {
  params->type = type;

  if (!new_config.is_valid()) {
    LOG(ERROR) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
               << " Invalid configuration received";
    params->status = MediaDataStatus::kMediaError;
    return;
  }

  *current_config = new_config;
  params->status = MediaDataStatus::kConfigChanged;
}

}  // namespace

using media::DemuxerStream;

IPCMediaPipelineHostImpl::IPCMediaPipelineHostImpl(
    gpu::GpuChannelHost* channel,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    GpuVideoAcceleratorFactories* factories,
    DataSource* data_source)
    : task_runner_(task_runner),
      pipeline_data_(data_source, channel),
      channel_(channel),
      routing_id_(MSG_ROUTING_NONE),
      factories_(factories),
      weak_ptr_factory_(this) {
  DCHECK(channel_.get());
  LOG(INFO) << " PROPMEDIA(RENDERER) : " << __FUNCTION__;
}

IPCMediaPipelineHostImpl::~IPCMediaPipelineHostImpl() {
  // We use WeakPtrs which require that we (i.e. our |weak_ptr_factory_|) are
  // destroyed on the same thread they are used.
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (is_connected()) {
    LOG(ERROR) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
               << " Object was not brought down properly. Missing "
               << " MediaPipelineMsg_Stopped?";
  }
}

int32_t IPCMediaPipelineHostImpl::command_buffer_route_id() const {

    if (!factories_)
        return MSG_ROUTING_NONE;

    int32_t factories_route_id = factories_->GetCommandBufferRouteId();

    if (!factories_route_id)
        return MSG_ROUTING_NONE;

    return factories_route_id;
}

void IPCMediaPipelineHostImpl::Initialize(const std::string& mimetype,
                                          const InitializeCB& callback) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!is_connected());
  DCHECK(init_callback_.is_null());

  VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
          << " With Mimetype : " << mimetype;

  routing_id_ = channel_->GenerateRouteID();

  VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
          << " Create pipeline";

  if (!channel_->Send(new MediaPipelineMsg_New(
          routing_id_, command_buffer_route_id()))) {
    callback.Run(false, -1, PlatformMediaTimeInfo(),
                 PlatformAudioConfig(), PlatformVideoConfig());
    return;
  }

  channel_->AddRoute(routing_id_, weak_ptr_factory_.GetWeakPtr());

  init_callback_ = callback;
  int64_t size = -1;
  if (!pipeline_data_.GetSizeSource(&size))
    size = -1;

  VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
          << " Initialize pipeline - size of source : " << size;

  channel_->Send(new MediaPipelineMsg_Initialize(
      routing_id_, size, pipeline_data_.IsStreamingSource(), mimetype));
}

void IPCMediaPipelineHostImpl::OnInitialized(
    bool success,
    int bitrate,
    const PlatformMediaTimeInfo& time_info,
    const PlatformAudioConfig& audio_config,
    const PlatformVideoConfig& video_config) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (init_callback_.is_null()) {
    LOG(ERROR) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
               << " Unexpected call to " << __FUNCTION__;
    return;
  }

  if (audio_config.is_valid()) {
    VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
            << " Audio Config Acceptable : "
            << Loggable(audio_config);
    audio_config_ = audio_config;
  } else {
    LOG(WARNING) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
                 << " Audio Config is not Valid "
                 << Loggable(audio_config);
  }

  if (video_config.is_valid()) {
    video_config_ = video_config;
  } else {
    LOG(WARNING) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
                 << " Video Config is not Valid "
                 << Loggable(video_config);
  }

  success = success && bitrate >= 0;

  base::ResetAndReturn(&init_callback_)
      .Run(success, bitrate, time_info, audio_config, video_config);
}

void IPCMediaPipelineHostImpl::OnRequestBufferForRawData(
    size_t requested_size) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (pipeline_data_.PrepareRaw(requested_size)) {
    channel_->Send(new MediaPipelineMsg_BufferForRawDataReady(
        routing_id_,
        pipeline_data_.MappedSizeRaw(),
        channel_->ShareToGpuProcess(pipeline_data_.HandleRaw())));
  } else {
    channel_->Send(new MediaPipelineMsg_BufferForRawDataReady(
        routing_id_, 0, base::SharedMemoryHandle()));
  }
}

void IPCMediaPipelineHostImpl::OnRequestBufferForDecodedData(
    PlatformMediaDataType type,
    size_t requested_size) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (!is_read_in_progress(type)) {
    LOG(ERROR) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
               << " Unexpected call to " << __FUNCTION__;
    return;
  }

  if (pipeline_data_.PrepareDecoded(type, requested_size)) {
    channel_->Send(new MediaPipelineMsg_BufferForDecodedDataReady(
        routing_id_,
        type,
        pipeline_data_.MappedSizeDecoded(type),
        channel_->ShareToGpuProcess(pipeline_data_.HandleDecoded(type))));
  } else {
    channel_->Send(new MediaPipelineMsg_BufferForDecodedDataReady(
        routing_id_, type, 0, base::SharedMemoryHandle()));
  }
}

void IPCMediaPipelineHostImpl::StartWaitingForSeek() {
  channel_->Send(new MediaPipelineMsg_WillSeek(routing_id_));
}

void IPCMediaPipelineHostImpl::Seek(base::TimeDelta time,
                                    const PipelineStatusCB& status_cb) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(is_connected());
  DCHECK(seek_callback_.is_null());

  TRACE_EVENT_ASYNC_BEGIN0("IPC_MEDIA", "Seek", this);

  seek_callback_ = status_cb;
  channel_->Send(new MediaPipelineMsg_Seek(routing_id_, time));
}

void IPCMediaPipelineHostImpl::OnSought(bool success) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (seek_callback_.is_null()) {
    LOG(ERROR) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
               << " Unexpected call to " << __FUNCTION__;
    return;
  }

  LOG_IF(WARNING, !success)
      << " PROPMEDIA(RENDERER) : " << __FUNCTION__
      << " PIPELINE_ERROR_ABORT";

  base::ResetAndReturn(&seek_callback_)
      .Run(success ? PipelineStatus::PIPELINE_OK : PipelineStatus::PIPELINE_ERROR_ABORT);

  TRACE_EVENT_ASYNC_END0("IPC_MEDIA", "Seek", this);
}

void IPCMediaPipelineHostImpl::Stop() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(is_connected());

  TRACE_EVENT0("IPC_MEDIA", "Stop");

  channel_->Send(new MediaPipelineMsg_Stop(routing_id_));

  channel_->Send(new MediaPipelineMsg_Destroy(routing_id_));
  channel_->RemoveRoute(routing_id_);
  routing_id_ = MSG_ROUTING_NONE;

  pipeline_data_.StopSource();
}

void IPCMediaPipelineHostImpl::ReadDecodedData(
    PlatformMediaDataType type,
    const DemuxerStream::ReadCB& read_cb) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!is_read_in_progress(type)) << "Overlapping reads are not supported";
  DCHECK(is_connected());

  TRACE_EVENT_ASYNC_BEGIN0(
      "IPC_MEDIA", kDecodedDataReadTraceEventNames[type], this);

  if (!channel_->Send(
          new MediaPipelineMsg_ReadDecodedData(routing_id_, type))) {
    read_cb.Run(DemuxerStream::kAborted, NULL);
    return;
  }

  decoded_data_read_callbacks_[type] = read_cb;
}


void IPCMediaPipelineHostImpl::AppendBuffer(
        const scoped_refptr<DecoderBuffer>& buffer,
        const VideoDecoder::DecodeCB& decode_cb) {
  LOG(INFO) << " PROPMEDIA(RENDERER) : " << __FUNCTION__;
  pipeline_data_.AppendBuffer(buffer, decode_cb);
}

bool IPCMediaPipelineHostImpl::DecodeVideo(const VideoDecoderConfig& config,
                                           const DecodeVideoCB& read_cb) {
  LOG(INFO) << " PROPMEDIA(RENDERER) : " << __FUNCTION__;
  config_ = config;
  decoded_video_frame_callback_ = read_cb;
  ReadDecodedData(PlatformMediaDataType::PLATFORM_MEDIA_VIDEO,
                  base::Bind(&IPCMediaPipelineHostImpl::DecodedVideoReady,
                             base::Unretained(this)));
  return true;
}

void IPCMediaPipelineHostImpl::DecodedVideoReady(
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

bool IPCMediaPipelineHostImpl::HasEnoughData() {
  LOG(INFO) << " PROPMEDIA(RENDERER) : " << __FUNCTION__;
  return pipeline_data_.HasEnoughData();
}

int IPCMediaPipelineHostImpl::GetMaxDecodeBuffers() {
  LOG(INFO) << " PROPMEDIA(RENDERER) : " << __FUNCTION__;
  return pipeline_data_.GetMaxDecodeBuffers();
}

void IPCMediaPipelineHostImpl::OnReadRawData(int64_t position, int size) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  TRACE_EVENT_ASYNC_BEGIN0("IPC_MEDIA", "ReadRawData", this);

  if (!pipeline_data_.IsSufficientRaw(size)) {
    LOG(ERROR) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
               << " Unexpected call to " << __FUNCTION__;
    channel_->Send(new MediaPipelineMsg_RawDataReady(
        routing_id_, DataSource::kReadError));
    TRACE_EVENT_ASYNC_END0("IPC_MEDIA", "ReadRawData", this);
  }

  pipeline_data_.ReadFromSource(position,
                    size,
                    pipeline_data_.MemoryRaw(),
                    BindToCurrentLoop(base::Bind(
                        &IPCMediaPipelineHostImpl::OnReadRawDataFinished,
                        weak_ptr_factory_.GetWeakPtr())));
}

void IPCMediaPipelineHostImpl::OnReadRawDataFinished(int size) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(pipeline_data_.IsSufficientRaw(size) ||
         size == DataSource::kReadError);

  if (!is_connected())
    // Someone called Stop() after we got the ReadRawData message and beat us
    // to it.
    return;

  channel_->Send(new MediaPipelineMsg_RawDataReady(routing_id_, size));
  TRACE_EVENT_ASYNC_END0("IPC_MEDIA", "ReadRawData", this);
}

bool IPCMediaPipelineHostImpl::OnMessageReceived(const IPC::Message& msg) {
  // GpuChannelHost gives us messages for handling on the thread that called
  // AddRoute().
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(IPCMediaPipelineHostImpl, msg)
    IPC_MESSAGE_HANDLER(MediaPipelineMsg_RequestBufferForDecodedData,
                        OnRequestBufferForDecodedData)
    IPC_MESSAGE_HANDLER(MediaPipelineMsg_RequestBufferForRawData,
                        OnRequestBufferForRawData)
    IPC_MESSAGE_HANDLER(MediaPipelineMsg_ReadRawData, OnReadRawData)
    IPC_MESSAGE_HANDLER(MediaPipelineMsg_DecodedDataReady, OnDecodedDataReady)
    IPC_MESSAGE_HANDLER(MediaPipelineMsg_Initialized, OnInitialized)
    IPC_MESSAGE_HANDLER(MediaPipelineMsg_Sought, OnSought)
    IPC_MESSAGE_HANDLER(MediaPipelineMsg_AudioConfigChanged,
                        OnAudioConfigChanged)
    IPC_MESSAGE_HANDLER(MediaPipelineMsg_VideoConfigChanged,
                        OnVideoConfigChanged)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  DCHECK(handled) << msg.type();
  return handled;
}

bool IPCMediaPipelineHostImpl::is_connected() const {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  return routing_id_ != MSG_ROUTING_NONE;
}

void IPCMediaPipelineHostImpl::OnDecodedDataReady(
    const MediaPipelineMsg_DecodedDataReady_Params& params) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (!is_read_in_progress(params.type)) {
    LOG(ERROR) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
               << " Unexpected call to " << __FUNCTION__;
    return;
  }

  switch (params.status) {
    case MediaDataStatus::kOk: {
      scoped_refptr<DecoderBuffer> buffer;
      if (!pipeline_data_.IsSufficientDecoded(params.type, params.size)) {
        LOG(ERROR) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
                    << " Unexpected call to " << __FUNCTION__;
        return;
      }
      buffer = DecoderBuffer::CopyFrom(
          pipeline_data_.MemoryDecoded(params.type), params.size);

      buffer->set_timestamp(params.timestamp);
      buffer->set_duration(params.duration);

      base::ResetAndReturn(&decoded_data_read_callbacks_[params.type])
          .Run(DemuxerStream::kOk, buffer);
      break;
    }

    case MediaDataStatus::kEOS:
      base::ResetAndReturn(&decoded_data_read_callbacks_[params.type])
          .Run(DemuxerStream::kOk, DecoderBuffer::CreateEOSBuffer());
      break;

    case MediaDataStatus::kConfigChanged:
      base::ResetAndReturn(&decoded_data_read_callbacks_[params.type])
          .Run(DemuxerStream::kConfigChanged, nullptr);
      break;

    case MediaDataStatus::kMediaError:
      // Note that this is a decoder error rather than demuxer error.  Don't
      // return DemuxerStream::kAborted.  Instead, return an empty buffer so
      // that the decoder can signal a decoder error.
      base::ResetAndReturn(&decoded_data_read_callbacks_[params.type])
          .Run(DemuxerStream::kOk, new DecoderBuffer(0));
      break;

    default:
      NOTREACHED();
      break;
  }

  TRACE_EVENT_ASYNC_END0(
      "IPC_MEDIA", kDecodedDataReadTraceEventNames[params.type], this);
}

void IPCMediaPipelineHostImpl::OnAudioConfigChanged(
    const PlatformAudioConfig& new_audio_config) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (!is_read_in_progress(PlatformMediaDataType::PLATFORM_MEDIA_AUDIO)) {
    LOG(ERROR) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
               << " Unexpected call to " << __FUNCTION__;
    return;
  }

  VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
          << " Previous Config "
          << Loggable(audio_config_);
  VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
          << " New Config "
          << Loggable(new_audio_config);

  MediaPipelineMsg_DecodedDataReady_Params params;
  HandleConfigChange<PlatformMediaDataType::PLATFORM_MEDIA_AUDIO>(new_audio_config,
                                                  &audio_config_, &params);
  OnDecodedDataReady(params);
}

void IPCMediaPipelineHostImpl::OnVideoConfigChanged(
    const PlatformVideoConfig& new_video_config) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (!is_read_in_progress(PlatformMediaDataType::PLATFORM_MEDIA_VIDEO)) {
    LOG(ERROR) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
               << " Unexpected call to " << __FUNCTION__;
    return;
  }

  MediaPipelineMsg_DecodedDataReady_Params params;
  VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
          << " Previous Config "
          << Loggable(video_config_);
  VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
          << " New Config "
          << Loggable(new_video_config);
  HandleConfigChange<PlatformMediaDataType::PLATFORM_MEDIA_VIDEO>(new_video_config,
                                                  &video_config_, &params);

  OnDecodedDataReady(params);
}

PlatformAudioConfig IPCMediaPipelineHostImpl::audio_config() const {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  return audio_config_;
}

PlatformVideoConfig IPCMediaPipelineHostImpl::video_config() const {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  return video_config_;
}

}  // namespace media
