// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#include "platform_media/gpu/pipeline/ipc_media_pipeline.h"

#include <algorithm>

#include "base/command_line.h"
#include "base/numerics/safe_conversions.h"
#include "base/stl_util.h"
#include "base/trace_event/trace_event.h"
#include "ipc/ipc_sender.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/data_buffer.h"
#include "mojo/public/cpp/base/shared_memory_utils.h"

#include "platform_media/common/media_pipeline_messages.h"
#include "platform_media/common/platform_ipc_util.h"
#include "platform_media/common/platform_logging_util.h"
#include "platform_media/common/platform_media_pipeline_types.h"
#include "platform_media/gpu/data_source/ipc_data_source_impl.h"
#include "platform_media/gpu/pipeline/platform_media_pipeline.h"

namespace media {

namespace {

constexpr const char* GetDecodeDataReadTraceEventName(PlatformStreamType type) {
  switch (type) {
    case PlatformStreamType::kAudio:
      return "GPU ReadAudioData";
    case PlatformStreamType::kVideo:
      return "GPU ReadVideoData";
  }
}

}  // namespace

IPCMediaPipeline::IPCMediaPipeline() {
  std::fill(std::begin(has_media_type_), std::end(has_media_type_), false);

  IPCDecodingBuffer::ReplyCB reply_cb = base::BindRepeating(
      &IPCMediaPipeline::DecodedDataReady, weak_ptr_factory_.GetWeakPtr());
  for (PlatformStreamType stream_type : AllStreamTypes()) {
    GetElem(ipc_decoding_buffers_, stream_type).Init(stream_type);
    GetElem(ipc_decoding_buffers_, stream_type).set_reply_cb(reply_cb);
  }
}

IPCMediaPipeline::~IPCMediaPipeline() {
  VLOG(2) << " PROPMEDIA(GPU) : " << __FUNCTION__
          << " routing_id=" << routing_id_;
  DCHECK(thread_checker_.CalledOnValidThread());
  if (data_source_) {
    // In case of abrupt termination like after the renderer process crash the
    // data_source_ here may still have a pending callback. Call it before the
    // destructor for media_pipeline_ is called to ensure that the callback is
    // called before the pipeline destructor.
    data_source_->Stop();
  }
}

/* static */
std::unique_ptr<gpu::PropmediaGpuChannel::PipelineBase>
IPCMediaPipeline::Create() {
  return std::make_unique<IPCMediaPipeline>();
}

/* static */
void IPCMediaPipeline::ReadRawData(base::WeakPtr<IPCMediaPipeline> pipeline,
                                   ipc_data_source::Buffer source_buffer) {
  if (pipeline) {
    DCHECK_CALLED_ON_VALID_THREAD(pipeline->thread_checker_);
    if (pipeline->data_source_) {
      pipeline->data_source_->Read(std::move(source_buffer));
      return;
    }
  }
  source_buffer.SetReadError();
  ipc_data_source::Buffer::SendReply(std::move(source_buffer));
}

void IPCMediaPipeline::Initialize(
    IPC::Sender* channel,
    gpu::mojom::VivaldiMediaPipelineParamsPtr params) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(channel);
  DCHECK(!channel_);
  DCHECK_EQ(state_, CONSTRUCTED);
  VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
          << " route_id=" << params->route_id
          << " data_size=" << params->data_source_size
          << " streaming=" << params->is_data_source_streaming
          << " mime_type=" << params->mime_type;

  channel_ = channel;
  routing_id_ = params->route_id;
  do {
    base::ReadOnlySharedMemoryMapping data_source_mapping =
        params->data_source_buffer.Map();
    if (!data_source_mapping.IsValid()) {
      LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__
                 << " failed to map data source region";
      break;
    }

    VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
            << " Creating the PlatformMediaPipeline";
    media_pipeline_ = PlatformMediaPipeline::Create();

    if (!media_pipeline_)
      break;

    VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
            << " Creating the IPCDataSource";
    data_source_ = std::make_unique<IPCDataSourceImpl>(channel_, routing_id_);

    ipc_data_source::Info source_info;
    source_info.is_streaming = params->is_data_source_streaming;
    source_info.size = params->data_source_size;
    source_info.mime_type = std::move(params->mime_type);

    source_info.buffer.Init(
        std::move(data_source_mapping),
        base::BindRepeating(&IPCMediaPipeline::ReadRawData,
                            weak_ptr_factory_.GetWeakPtr()));

    state_ = BUSY;
    media_pipeline_->Initialize(std::move(source_info),
                                base::BindOnce(&IPCMediaPipeline::Initialized,
                                               weak_ptr_factory_.GetWeakPtr()));
    return;
  } while (false);

  channel_->Send(new MediaPipelineMsg_Initialized(
      routing_id_, false, 0, PlatformMediaTimeInfo(), PlatformAudioConfig(),
      PlatformVideoConfig()));
}

void IPCMediaPipeline::Initialized(bool success,
                                   int bitrate,
                                   const PlatformMediaTimeInfo& time_info,
                                   const PlatformAudioConfig& audio_config,
                                   const PlatformVideoConfig& video_config) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(state_, BUSY);

  VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__ << " ( success = " << success
          << " )" << Loggable(audio_config);

  GetElem(has_media_type_, PlatformStreamType::kAudio) =
      audio_config.is_valid();
  GetElem(has_media_type_, PlatformStreamType::kVideo) =
      video_config.is_valid();

  channel_->Send(new MediaPipelineMsg_Initialized(
      routing_id_, success, bitrate, time_info, audio_config, video_config));

  state_ = success ? DECODING : STOPPED;
}

void IPCMediaPipeline::OnReadDecodedData(PlatformStreamType stream_type) {
  DCHECK(thread_checker_.CalledOnValidThread());
  TRACE_EVENT0("IPC_MEDIA", "IPCMediaPipeline::OnReadDecodedData");
  VLOG(7) << " PROPMEDIA(GPU) : " << __FUNCTION__
          << " stream_type=" << GetStreamTypeName(stream_type);

  // We must be in the decoding state and not already running an asynchronous
  // call to decode data of this type.
  if (state_ != DECODING || !GetElem(ipc_decoding_buffers_, stream_type)) {
    LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__ << " Unexpected call to "
               << __FUNCTION__;
    return;
  }
  if (!has_media_type(stream_type)) {
    LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__
               << " No data of given media kind ("
               << GetStreamTypeName(stream_type) << ") to decode";
    return;
  }

  TRACE_EVENT_ASYNC_BEGIN0("IPC_MEDIA",
                           GetDecodeDataReadTraceEventName(stream_type), this);

  IPCDecodingBuffer buffer =
      std::move(GetElem(ipc_decoding_buffers_, stream_type));
  media_pipeline_->ReadMediaData(std::move(buffer));
}

void IPCMediaPipeline::DecodedDataReady(IPCDecodingBuffer buffer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(state_, DECODING);

  PlatformStreamType stream_type = buffer.stream_type();
  DCHECK(!GetElem(ipc_decoding_buffers_, stream_type));
  DCHECK(buffer);

  VLOG(7) << " PROPMEDIA(GPU) : " << __FUNCTION__
          << " stream_type=" << GetStreamTypeName(stream_type)
          << " status=" << static_cast<int>(buffer.status())
          << " data_size=" << buffer.data_size();
  if (buffer.status() == MediaDataStatus::kConfigChanged) {
    switch (stream_type) {
      case PlatformStreamType::kAudio:
        DCHECK(buffer.GetAudioConfig().is_valid());
        channel_->Send(new MediaPipelineMsg_AudioConfigChanged(
            routing_id_, buffer.GetAudioConfig()));
        break;
      case PlatformStreamType::kVideo:
        DCHECK(buffer.GetVideoConfig().is_valid());
        channel_->Send(new MediaPipelineMsg_VideoConfigChanged(
            routing_id_, buffer.GetVideoConfig()));
        break;
    }
  } else {
    MediaPipelineMsg_DecodedDataReady_Params reply_params;
    reply_params.stream_type = stream_type;
    reply_params.status = buffer.status();
    base::ReadOnlySharedMemoryRegion region_to_send;
    switch (buffer.status()) {
      case MediaDataStatus::kEOS:
        break;
      case MediaDataStatus::kMediaError:
        LOG(WARNING) << " PROPMEDIA(GPU) : " << __FUNCTION__
                     << " status : MediaDataStatus::kMediaError";
        break;
      case MediaDataStatus::kConfigChanged:
        NOTREACHED();
        break;
      case MediaDataStatus::kOk:
        reply_params.size = buffer.data_size();
        reply_params.timestamp = buffer.timestamp();
        reply_params.duration = buffer.duration();
        region_to_send = buffer.extract_region_to_send();
        break;
    }

    channel_->Send(new MediaPipelineMsg_DecodedDataReady(
        routing_id_, reply_params, std::move(region_to_send)));
  }

  TRACE_EVENT_ASYNC_END0("IPC_MEDIA",
                         GetDecodeDataReadTraceEventName(stream_type), this);

  // Reuse the buffer the next time.
  GetElem(ipc_decoding_buffers_, stream_type) = std::move(buffer);
}

void IPCMediaPipeline::OnWillSeek() {
  DCHECK(thread_checker_.CalledOnValidThread());
  VLOG(3) << " PROPMEDIA(GPU) : " << __FUNCTION__ << " reading_audio="
          << !GetElem(ipc_decoding_buffers_, PlatformStreamType::kAudio)
          << " reading_video="
          << !GetElem(ipc_decoding_buffers_, PlatformStreamType::kVideo);

  media_pipeline_->WillSeek();
}

void IPCMediaPipeline::OnSeek(base::TimeDelta time) {
  DCHECK(thread_checker_.CalledOnValidThread());
  VLOG(3) << " PROPMEDIA(GPU) : " << __FUNCTION__ << " reading_audio="
          << !GetElem(ipc_decoding_buffers_, PlatformStreamType::kAudio)
          << " reading_video="
          << !GetElem(ipc_decoding_buffers_, PlatformStreamType::kVideo);

  if (state_ != DECODING) {
    LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__ << " Unexpected call to "
               << __FUNCTION__;
    return;
  }
  state_ = BUSY;

  media_pipeline_->Seek(time,
                        base::BindRepeating(&IPCMediaPipeline::SeekDone,
                                            weak_ptr_factory_.GetWeakPtr()));
}

void IPCMediaPipeline::SeekDone(bool success) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(state_, BUSY);

  channel_->Send(new MediaPipelineMsg_Sought(routing_id_, success));

  state_ = DECODING;
}

bool IPCMediaPipeline::OnMessageReceived(const IPC::Message& msg) {
  DCHECK(thread_checker_.CalledOnValidThread());

  bool handled = true;
  TRACE_EVENT0("IPC_MEDIA", "IPCMediaPipeline::OnMessageReceived");
  IPC_BEGIN_MESSAGE_MAP(IPCMediaPipeline, msg)
    IPC_MESSAGE_FORWARD(MediaPipelineMsg_RawDataReady, data_source_.get(),
                        IPCDataSourceImpl::OnRawDataReady)
    IPC_MESSAGE_HANDLER(MediaPipelineMsg_ReadDecodedData, OnReadDecodedData)
    IPC_MESSAGE_HANDLER(MediaPipelineMsg_WillSeek, OnWillSeek)
    IPC_MESSAGE_HANDLER(MediaPipelineMsg_Seek, OnSeek)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  DCHECK(handled) << msg.type();
  return handled;
}

}  // namespace media
