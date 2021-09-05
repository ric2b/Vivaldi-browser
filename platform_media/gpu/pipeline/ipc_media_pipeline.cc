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
#include "media/base/data_buffer.h"
#include "media/base/bind_to_current_loop.h"
#include "mojo/public/cpp/base/shared_memory_utils.h"

#include "platform_media/common/media_pipeline_messages.h"
#include "platform_media/common/platform_ipc_util.h"
#include "platform_media/common/platform_logging_util.h"
#include "platform_media/common/platform_media_pipeline_types.h"
#include "platform_media/gpu/data_source/ipc_data_source_impl.h"
#include "platform_media/gpu/pipeline/platform_media_pipeline_factory.h"
#include "platform_media/gpu/pipeline/platform_media_pipeline.h"

namespace media {

namespace {

const char* const kDecodedDataReadTraceEventNames[] = {"GPU ReadAudioData",
                                                       "GPU ReadVideoData"};
static_assert(base::size(kDecodedDataReadTraceEventNames) ==
                  static_cast<size_t>(kPlatformMediaDataTypeCount),
              "Incorrect number of defined tracing event names.");
}  // namespace

IPCMediaPipeline::IPCMediaPipeline(
    IPC::Sender* channel,
    int32_t routing_id,
    PlatformMediaPipelineFactory* pipeline_factory)
    : state_(CONSTRUCTED),
      channel_(channel),
      routing_id_(routing_id),
      pipeline_factory_(pipeline_factory),
      weak_ptr_factory_(this) {
  VLOG(2) << " PROPMEDIA(GPU) : " << __FUNCTION__
          << " routing_id=" << routing_id;
  DCHECK(channel_);

  std::fill(std::begin(has_media_type_), std::end(has_media_type_), false);

  IPCDecodingBuffer::ReplyCB reply_cb = base::Bind(
      &IPCMediaPipeline::DecodedDataReady, weak_ptr_factory_.GetWeakPtr());
  for (int i = 0; i < kPlatformMediaDataTypeCount; ++i) {
    ipc_decoding_buffers_[i].Init(static_cast<PlatformMediaDataType>(i));
    ipc_decoding_buffers_[i].set_reply_cb(reply_cb);
  }
}

IPCMediaPipeline::~IPCMediaPipeline() {
  VLOG(2) << " PROPMEDIA(GPU) : " << __FUNCTION__
          << " routing_id=" << routing_id_;
  DCHECK(thread_checker_.CalledOnValidThread());
  if (data_source_) {
    // In case of abrupt termination like after the renderer process crash the
    // data_source_ here may still have a pending callback. Call it before the
    // destructor for media_pipeline_ is called to ensure ordered shutdown.
    data_source_->Stop();
  }
}

// static
void IPCMediaPipeline::ReadRawData(base::WeakPtr<IPCMediaPipeline> pipeline,
                                   int64_t position,
                                   int size,
                                   ipc_data_source::ReadCB read_cb) {
  if (pipeline) {
    DCHECK_CALLED_ON_VALID_THREAD(pipeline->thread_checker_);
    if (pipeline->data_source_) {
      pipeline->data_source_->Read(position, size, std::move(read_cb));
      return;
    }
  }
  std::move(read_cb).Run(ipc_data_source::kReadError, nullptr);
}

void IPCMediaPipeline::OnInitialize(int64_t data_source_size,
                                    bool is_data_source_streaming,
                                    const std::string& mime_type) {
  VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
          << " data_size=" << data_source_size
          << " streaming=" << is_data_source_streaming
          << " mime_type=" << mime_type;
  DCHECK(thread_checker_.CalledOnValidThread());

  if (state_ != CONSTRUCTED) {
    LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__
               << " Unexpected call to " << __FUNCTION__;
    return;
  }

  VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
          << " Creating the PlatformMediaPipeline";
  media_pipeline_ = pipeline_factory_->CreatePipeline();
  if (!media_pipeline_) {
    channel_->Send(new MediaPipelineMsg_Initialized(
        routing_id_, false, 0, PlatformMediaTimeInfo(), PlatformAudioConfig(),
        PlatformVideoConfig()));
    return;
  }

  VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
          << " Creating the IPCDataSource";
  data_source_ = std::make_unique<IPCDataSourceImpl>(channel_, routing_id_);

  ipc_data_source::Reader source_reader = base::BindRepeating(
      &IPCMediaPipeline::ReadRawData, weak_ptr_factory_.GetWeakPtr());

  ipc_data_source::Info source_info;
  source_info.is_streaming = is_data_source_streaming;
  source_info.size = data_source_size;
  source_info.mime_type = std::move(mime_type);

  state_ = BUSY;
  media_pipeline_->Initialize(std::move(source_reader), std::move(source_info),
                              base::Bind(&IPCMediaPipeline::Initialized,
                                         weak_ptr_factory_.GetWeakPtr()));
}

void IPCMediaPipeline::Initialized(
    bool success,
    int bitrate,
    const PlatformMediaTimeInfo& time_info,
    const PlatformAudioConfig& audio_config,
    const PlatformVideoConfig& video_config) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(state_, BUSY);

  VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
          << " ( success = " << success << " )"
          << Loggable(audio_config);

  has_media_type_[PlatformMediaDataType::PLATFORM_MEDIA_AUDIO] = audio_config.is_valid();
  has_media_type_[PlatformMediaDataType::PLATFORM_MEDIA_VIDEO] = video_config.is_valid();

  channel_->Send(new MediaPipelineMsg_Initialized(
      routing_id_, success, bitrate, time_info, audio_config, video_config));

  state_ = success ? DECODING : STOPPED;
}

void IPCMediaPipeline::OnReadDecodedData(PlatformMediaDataType type) {
  DCHECK(thread_checker_.CalledOnValidThread());
  TRACE_EVENT0("IPC_MEDIA", "IPCMediaPipeline::OnReadDecodedData");
  VLOG(7) << " PROPMEDIA(GPU) : " << __FUNCTION__ << " type=" << type;

  // We must be in the decoding state and not already running an asynchronous
  // call to decode data of this type.
  if (state_ != DECODING || !ipc_decoding_buffers_[type]) {
    LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__
               << " Unexpected call to " << __FUNCTION__;
    return;
  }
  if (!has_media_type(type)) {
    LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__
               << " No data of given media type (" << type << ") to decode";
    return;
  }

  TRACE_EVENT_ASYNC_BEGIN0(
      "IPC_MEDIA", kDecodedDataReadTraceEventNames[type], this);

  IPCDecodingBuffer buffer = std::move(ipc_decoding_buffers_[type]);
  media_pipeline_->ReadMediaData(std::move(buffer));
}

void IPCMediaPipeline::DecodedDataReady(IPCDecodingBuffer buffer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(state_, DECODING);
  DCHECK(!ipc_decoding_buffers_[buffer.type()]);
  DCHECK(buffer);

  PlatformMediaDataType type = buffer.type();
  VLOG(7) << " PROPMEDIA(GPU) : " << __FUNCTION__ << " type=" << type
          << " status=" << static_cast<int>(buffer.status())
          << " data_size=" << buffer.data_size();
  if (buffer.status() == MediaDataStatus::kConfigChanged) {
    switch (type) {
      case PlatformMediaDataType::PLATFORM_MEDIA_AUDIO:
        DCHECK(buffer.GetAudioConfig().is_valid());
        channel_->Send(new MediaPipelineMsg_AudioConfigChanged(
            routing_id_, buffer.GetAudioConfig()));
        break;
      case PlatformMediaDataType::PLATFORM_MEDIA_VIDEO:
        DCHECK(buffer.GetVideoConfig().is_valid());
        channel_->Send(new MediaPipelineMsg_VideoConfigChanged(
            routing_id_, buffer.GetVideoConfig()));
        break;
    }
  } else {
    MediaPipelineMsg_DecodedDataReady_Params reply_params;
    reply_params.type = type;
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

  TRACE_EVENT_ASYNC_END0(
      "IPC_MEDIA", kDecodedDataReadTraceEventNames[type], this);

  // Reuse the buffer the next time.
  ipc_decoding_buffers_[type] = std::move(buffer);
}

void IPCMediaPipeline::OnWillSeek() {
  DCHECK(thread_checker_.CalledOnValidThread());
  VLOG(3)
      << " PROPMEDIA(GPU) : " << __FUNCTION__ << " reading_audio="
      << !ipc_decoding_buffers_[PlatformMediaDataType::PLATFORM_MEDIA_AUDIO]
      << " reading_video="
      << !ipc_decoding_buffers_[PlatformMediaDataType::PLATFORM_MEDIA_VIDEO];

  media_pipeline_->WillSeek();
}

void IPCMediaPipeline::OnSeek(base::TimeDelta time) {
  DCHECK(thread_checker_.CalledOnValidThread());
  VLOG(3)
      << " PROPMEDIA(GPU) : " << __FUNCTION__ << " reading_audio="
      << !ipc_decoding_buffers_[PlatformMediaDataType::PLATFORM_MEDIA_AUDIO]
      << " reading_video="
      << !ipc_decoding_buffers_[PlatformMediaDataType::PLATFORM_MEDIA_VIDEO];

  if (state_ != DECODING) {
    LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__
               << " Unexpected call to " << __FUNCTION__;
    return;
  }
  state_ = BUSY;

  media_pipeline_->Seek(
      time,
      base::Bind(&IPCMediaPipeline::SeekDone, weak_ptr_factory_.GetWeakPtr()));
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
    IPC_MESSAGE_FORWARD(MediaPipelineMsg_RawDataReady,
                        data_source_.get(),
                        IPCDataSourceImpl::OnRawDataReady)
    IPC_MESSAGE_HANDLER(MediaPipelineMsg_ReadDecodedData, OnReadDecodedData)
    IPC_MESSAGE_HANDLER(MediaPipelineMsg_Initialize, OnInitialize)
    IPC_MESSAGE_HANDLER(MediaPipelineMsg_WillSeek, OnWillSeek)
    IPC_MESSAGE_HANDLER(MediaPipelineMsg_Seek, OnSeek)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  DCHECK(handled) << msg.type();
  return handled;
}

}  // namespace media
