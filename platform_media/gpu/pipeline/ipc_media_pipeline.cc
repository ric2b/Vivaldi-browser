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
#include "base/trace_event/trace_event.h"
#include "platform_media/gpu/pipeline/platform_media_pipeline_create.h"
#include "platform_media/gpu/data_source/ipc_data_source_impl.h"
#include "platform_media/gpu/pipeline/platform_media_pipeline.h"
#include "platform_media/common/media_pipeline_messages.h"
#include "ipc/ipc_sender.h"
#include "media/base/data_buffer.h"

#include "platform_media/common/platform_logging_util.h"
#include "platform_media/common/platform_media_pipeline_types.h"


namespace media {

namespace {

const char* const kDecodedDataReadTraceEventNames[] = {"GPU ReadAudioData",
                                                       "GPU ReadVideoData"};
static_assert(arraysize(kDecodedDataReadTraceEventNames) == static_cast<size_t>(
  PlatformMediaDataType::PLATFORM_MEDIA_DATA_TYPE_COUNT),
              "Incorrect number of defined tracing event names.");
}  // namespace

IPCMediaPipeline::IPCMediaPipeline(IPC::Sender* channel,
                                   int32_t routing_id)
    : state_(CONSTRUCTED),
      channel_(channel),
      routing_id_(routing_id),
      weak_ptr_factory_(this) {
  DCHECK(channel_);

  std::fill(has_media_type_,
            has_media_type_ +
                static_cast<size_t>(
                  PlatformMediaDataType::PLATFORM_MEDIA_DATA_TYPE_COUNT),
            false);
}

IPCMediaPipeline::~IPCMediaPipeline() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void IPCMediaPipeline::OnInitialize(int64_t data_source_size,
                                    bool is_data_source_streaming,
                                    const std::string& mime_type) {
  VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());

  if (state_ != CONSTRUCTED) {
    LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__
               << " Unexpected call to " << __FUNCTION__;
    return;
  }
  state_ = BUSY;

  VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
          << " Creating the IPCDataSource";
  data_source_.reset(new IPCDataSourceImpl(
      channel_, routing_id_, data_source_size, is_data_source_streaming));

  VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
          << " Creating the PlatformMediaPipeline";
  media_pipeline_.reset(PlatformMediaPipelineCreate(
      data_source_.get(), base::Bind(&IPCMediaPipeline::OnAudioConfigChanged,
                                     weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&IPCMediaPipeline::OnVideoConfigChanged,
                 weak_ptr_factory_.GetWeakPtr())));

  media_pipeline_->Initialize(mime_type,
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

void IPCMediaPipeline::OnBufferForDecodedDataReady(
    PlatformMediaDataType type,
    size_t buffer_size,
    base::SharedMemoryHandle handle) {
  if (!pending_output_buffers_[type]) {
    LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__
               << " Unexpected call to " << __FUNCTION__;
    DecodedDataReady(type, nullptr);
    return;
  }

  DCHECK(!pending_output_buffers_[type]->end_of_stream());

  if (base::SharedMemory::IsHandleValid(handle)) {
    shared_decoded_data_[type].reset(new base::SharedMemory(handle, false));
    if (!shared_decoded_data_[type]->Map(buffer_size) ||
        shared_decoded_data_[type]->mapped_size() <
            base::saturated_cast<size_t>(
                pending_output_buffers_[type]->data_size())) {
      shared_decoded_data_[type].reset(nullptr);
    }
  } else {
    shared_decoded_data_[type].reset(nullptr);
  }

  scoped_refptr<DataBuffer> buffer;
  if (shared_decoded_data_[type])
    buffer = pending_output_buffers_[type];
  pending_output_buffers_[type] = nullptr;

  DecodedDataReady(type, buffer);
}

void IPCMediaPipeline::DecodedDataReady(
    PlatformMediaDataType type,
    const scoped_refptr<DataBuffer>& buffer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DecodedDataReadyImpl(type, buffer);
}

void IPCMediaPipeline::DecodedDataReadyImpl(
    PlatformMediaDataType type,
    const scoped_refptr<DataBuffer>& buffer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(state_, DECODING);
  DCHECK(!pending_output_buffers_[type]);

  MediaPipelineMsg_DecodedDataReady_Params reply_params;
  reply_params.type = type;

  if (buffer.get() == NULL) {
    LOG(WARNING) << " PROPMEDIA(GPU) : " << __FUNCTION__
                 << " status : MediaDataStatus::kMediaError";
    reply_params.status = MediaDataStatus::kMediaError;
  } else if (buffer->end_of_stream()) {
    reply_params.status = MediaDataStatus::kEOS;
  } else {
    if (!shared_decoded_data_[type] ||
        base::saturated_cast<int>(shared_decoded_data_[type]->mapped_size()) <
            buffer->data_size()) {
      pending_output_buffers_[type] = buffer;
      channel_->Send(new MediaPipelineMsg_RequestBufferForDecodedData(
          routing_id_, type, buffer->data_size()));
      return;
    }

    memcpy(shared_decoded_data_[type]->memory(), buffer->data(),
            buffer->data_size());

    reply_params.size = buffer->data_size();
    reply_params.status = MediaDataStatus::kOk;
    reply_params.timestamp = buffer->timestamp();
    reply_params.duration = buffer->duration();
  }

  channel_->Send(
      new MediaPipelineMsg_DecodedDataReady(routing_id_, reply_params));

  TRACE_EVENT_ASYNC_END0(
      "IPC_MEDIA", kDecodedDataReadTraceEventNames[type], this);
}

void IPCMediaPipeline::OnAudioConfigChanged(
    const PlatformAudioConfig& audio_config) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(state_, DECODING);
  DCHECK(audio_config.is_valid());

  VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
          << Loggable(audio_config);

  channel_->Send(
      new MediaPipelineMsg_AudioConfigChanged(routing_id_, audio_config));
}

void IPCMediaPipeline::OnVideoConfigChanged(
    const PlatformVideoConfig& video_config) {
  VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(state_, DECODING);
  DCHECK(video_config.is_valid());

  channel_->Send(
      new MediaPipelineMsg_VideoConfigChanged(routing_id_, video_config));
}

void IPCMediaPipeline::OnWillSeek() {
  DCHECK(thread_checker_.CalledOnValidThread());

  media_pipeline_->WillSeek();
}

void IPCMediaPipeline::OnSeek(base::TimeDelta time) {
  DCHECK(thread_checker_.CalledOnValidThread());

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

void IPCMediaPipeline::OnStop() {
  VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());

  media_pipeline_.reset();

  // We must not accept any reply callbacks once we are in the STOPPED state.
  weak_ptr_factory_.InvalidateWeakPtrs();

  state_ = STOPPED;
}

bool IPCMediaPipeline::OnMessageReceived(const IPC::Message& msg) {
  DCHECK(thread_checker_.CalledOnValidThread());

  bool handled = true;
  TRACE_EVENT0("IPC_MEDIA", "IPCMediaPipeline::OnMessageReceived");
  IPC_BEGIN_MESSAGE_MAP(IPCMediaPipeline, msg)
    IPC_MESSAGE_FORWARD(MediaPipelineMsg_BufferForRawDataReady,
                        data_source_.get(),
                        IPCDataSourceImpl::OnBufferForRawDataReady)
    IPC_MESSAGE_FORWARD(MediaPipelineMsg_RawDataReady,
                        data_source_.get(),
                        IPCDataSourceImpl::OnRawDataReady)
    IPC_MESSAGE_HANDLER(MediaPipelineMsg_BufferForDecodedDataReady,
                        OnBufferForDecodedDataReady)
    IPC_MESSAGE_HANDLER(MediaPipelineMsg_ReadDecodedData, OnReadDecodedData)
    IPC_MESSAGE_HANDLER(MediaPipelineMsg_Initialize, OnInitialize)
    IPC_MESSAGE_HANDLER(MediaPipelineMsg_WillSeek, OnWillSeek)
    IPC_MESSAGE_HANDLER(MediaPipelineMsg_Seek, OnSeek)
    IPC_MESSAGE_HANDLER(MediaPipelineMsg_Stop, OnStop)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  DCHECK(handled) << msg.type();
  return handled;
}

void IPCMediaPipeline::OnReadDecodedData(PlatformMediaDataType type) {
  DCHECK(thread_checker_.CalledOnValidThread());
  TRACE_EVENT0("IPC_MEDIA", "IPCMediaPipeline::OnReadDecodedData");

  if (state_ != DECODING) {
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

  const auto read_cb = base::Bind(&IPCMediaPipeline::DecodedDataReady,
                                  weak_ptr_factory_.GetWeakPtr(), type);
  if (type == PlatformMediaDataType::PLATFORM_MEDIA_AUDIO) {
    media_pipeline_->ReadAudioData(read_cb);
  } else {
    media_pipeline_->ReadVideoData(read_cb);
  }
}
}  // namespace media
