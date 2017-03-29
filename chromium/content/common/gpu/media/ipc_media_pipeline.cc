// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#include "content/common/gpu/media/ipc_media_pipeline.h"

#include <algorithm>

#include "base/command_line.h"
#include "base/numerics/safe_conversions.h"
#include "base/trace_event/trace_event.h"
#include "content/common/gpu/gpu_command_buffer_stub.h"
#include "content/common/gpu/media/ipc_data_source_impl.h"
#include "content/common/gpu/media/platform_media_pipeline.h"
#include "content/common/media/media_pipeline_messages.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "ipc/ipc_sender.h"
#include "media/base/data_buffer.h"
#include "media/filters/platform_media_pipeline_constants.h"
#include "media/filters/platform_media_pipeline_types.h"
#include "ui/gl/gl_surface_egl.h"

namespace {

const char* const kDecodedDataReadTraceEventNames[] = {"GPU ReadAudioData",
                                                       "GPU ReadVideoData"};
static_assert(arraysize(kDecodedDataReadTraceEventNames) ==
                  media::PLATFORM_MEDIA_DATA_TYPE_COUNT,
              "Incorrect number of defined tracing event names.");

bool MakeDecoderContextCurrent(
    const base::WeakPtr<content::GpuCommandBufferStub>& command_buffer) {
  if (!command_buffer) {
    DLOG(ERROR) << "Command buffer missing, can't make GL context current.";
    return false;
  }

  if (!command_buffer->decoder()->MakeCurrent()) {
    DLOG(ERROR) << "Failed to make GL context current.";
    return false;
  }

  return true;
}

}  // namespace

namespace content {

IPCMediaPipeline::IPCMediaPipeline(IPC::Sender* channel,
                                   int32 routing_id,
                                   GpuCommandBufferStub* command_buffer)
    : state_(CONSTRUCTED),
      channel_(channel),
      routing_id_(routing_id),
      command_buffer_(command_buffer),
      weak_ptr_factory_(this) {
  DCHECK(channel_);

  std::fill(has_media_type_,
            has_media_type_ + media::PLATFORM_MEDIA_DATA_TYPE_COUNT,
            false);
}

IPCMediaPipeline::~IPCMediaPipeline() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void IPCMediaPipeline::OnInitialize(int64 data_source_size,
                                    bool is_data_source_streaming,
                                    const std::string& mime_type) {
  DVLOG(1) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());

  if (state_ != CONSTRUCTED) {
    DLOG(ERROR) << "Unexpected call to " << __FUNCTION__;
    return;
  }
  state_ = BUSY;

  data_source_.reset(new IPCDataSourceImpl(
      channel_, routing_id_, data_source_size, is_data_source_streaming));

  media::PlatformMediaDecodingMode preferred_video_decoding_mode =
      media::PLATFORM_MEDIA_DECODING_MODE_SOFTWARE;
  PlatformMediaPipeline::MakeGLContextCurrentCB make_gl_context_current_cb;
  if (command_buffer_) {
    preferred_video_decoding_mode =
        media::PLATFORM_MEDIA_DECODING_MODE_HARDWARE;
    make_gl_context_current_cb =
        base::Bind(&MakeDecoderContextCurrent, command_buffer_->AsWeakPtr());
  }

  media_pipeline_.reset(PlatformMediaPipeline::Create(
      data_source_.get(), base::Bind(&IPCMediaPipeline::OnAudioConfigChanged,
                                     weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&IPCMediaPipeline::OnVideoConfigChanged,
                 weak_ptr_factory_.GetWeakPtr()),
      preferred_video_decoding_mode, make_gl_context_current_cb));

  media_pipeline_->Initialize(mime_type,
                              base::Bind(&IPCMediaPipeline::Initialized,
                                         weak_ptr_factory_.GetWeakPtr()));
}

void IPCMediaPipeline::Initialized(
    bool success,
    int bitrate,
    const media::PlatformMediaTimeInfo& time_info,
    const media::PlatformAudioConfig& audio_config,
    const media::PlatformVideoConfig& video_config) {
  DVLOG(1) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(state_, BUSY);

  has_media_type_[media::PLATFORM_MEDIA_AUDIO] = audio_config.is_valid();
  has_media_type_[media::PLATFORM_MEDIA_VIDEO] = video_config.is_valid();
  if (has_media_type_[media::PLATFORM_MEDIA_VIDEO])
    video_config_ = video_config;

  channel_->Send(new MediaPipelineMsg_Initialized(
      routing_id_, success, bitrate, time_info, audio_config, video_config));

  state_ = success ? DECODING : STOPPED;
}

void IPCMediaPipeline::OnBufferForDecodedDataReady(
    media::PlatformMediaDataType type,
    size_t buffer_size,
    base::SharedMemoryHandle handle) {
  if (!pending_output_buffers_[type]) {
    DLOG(ERROR) << "Unexpected call to " << __FUNCTION__;
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

  scoped_refptr<media::DataBuffer> buffer;
  if (shared_decoded_data_[type])
    buffer = pending_output_buffers_[type];
  pending_output_buffers_[type] = nullptr;

  DecodedDataReady(type, buffer);
}

void IPCMediaPipeline::DecodedDataReady(
    media::PlatformMediaDataType type,
    const scoped_refptr<media::DataBuffer>& buffer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!is_handling_accelerated_video_decode(type));

  const uint32_t dummy_client_texture_id = 0;
  DecodedDataReadyImpl(type, dummy_client_texture_id, buffer);
}

void IPCMediaPipeline::DecodedTextureReady(
    uint32_t client_texture_id,
    const scoped_refptr<media::DataBuffer>& buffer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(is_handling_accelerated_video_decode(media::PLATFORM_MEDIA_VIDEO));

  DecodedDataReadyImpl(media::PLATFORM_MEDIA_VIDEO, client_texture_id, buffer);
}

void IPCMediaPipeline::DecodedDataReadyImpl(
    media::PlatformMediaDataType type,
    uint32_t client_texture_id,
    const scoped_refptr<media::DataBuffer>& buffer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(state_, DECODING);
  DCHECK(!pending_output_buffers_[type]);

  MediaPipelineMsg_DecodedDataReady_Params reply_params;
  reply_params.type = type;

  if (buffer.get() == NULL) {
    reply_params.status = media::kError;
  } else if (buffer->end_of_stream()) {
    reply_params.status = media::kEOS;
  } else {
    if (is_handling_accelerated_video_decode(type)) {
      reply_params.client_texture_id = client_texture_id;
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
    }

    reply_params.size = buffer->data_size();
    reply_params.status = media::kOk;
    reply_params.timestamp = buffer->timestamp();
    reply_params.duration = buffer->duration();
  }

  channel_->Send(
      new MediaPipelineMsg_DecodedDataReady(routing_id_, reply_params));

  TRACE_EVENT_ASYNC_END0(
      "IPC_MEDIA", kDecodedDataReadTraceEventNames[type], this);
}

void IPCMediaPipeline::OnAudioConfigChanged(
    const media::PlatformAudioConfig& audio_config) {
  DVLOG(1) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(state_, DECODING);
  DCHECK(audio_config.is_valid());

  channel_->Send(
      new MediaPipelineMsg_AudioConfigChanged(routing_id_, audio_config));
}

void IPCMediaPipeline::OnVideoConfigChanged(
    const media::PlatformVideoConfig& video_config) {
  DVLOG(1) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(state_, DECODING);
  DCHECK(video_config.is_valid());

  video_config_ = video_config;

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
    DLOG(ERROR) << "Unexpected call to " << __FUNCTION__;
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
  DVLOG(1) << __FUNCTION__;
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

void IPCMediaPipeline::OnReadDecodedData(media::PlatformMediaDataType type,
                                         uint32_t client_texture_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  TRACE_EVENT0("IPC_MEDIA", "IPCMediaPipeline::OnReadDecodedData");

  if (state_ != DECODING) {
    DLOG(ERROR) << "Unexpected call to " << __FUNCTION__;
    return;
  }
  if (!has_media_type(type)) {
    DLOG(ERROR) << "No data of given media type (" << type << ") to decode";
    return;
  }

  TRACE_EVENT_ASYNC_BEGIN0(
      "IPC_MEDIA", kDecodedDataReadTraceEventNames[type], this);

  if (is_handling_accelerated_video_decode(type)) {
    uint32_t service_texture_id = 0;
    if (!ClientToServiceTextureId(client_texture_id, &service_texture_id)) {
      DLOG(ERROR) << "Error while translating texture ID=" << client_texture_id;
      DecodedTextureReady(client_texture_id, nullptr);
      return;
    }
    media_pipeline_->ReadVideoData(
        base::Bind(&IPCMediaPipeline::DecodedTextureReady,
                   weak_ptr_factory_.GetWeakPtr(), client_texture_id),
        service_texture_id);
    return;
  }

  const auto read_cb = base::Bind(&IPCMediaPipeline::DecodedDataReady,
                                  weak_ptr_factory_.GetWeakPtr(), type);
  if (type == media::PLATFORM_MEDIA_AUDIO) {
    media_pipeline_->ReadAudioData(read_cb);
  } else {
    const uint32_t dummy_service_texture_id = 0;
    media_pipeline_->ReadVideoData(read_cb, dummy_service_texture_id);
  }
}

bool IPCMediaPipeline::ClientToServiceTextureId(uint32_t client_texture_id,
                                                uint32_t* service_texture_id) {
  auto it = known_picture_buffers_.find(client_texture_id);
  if (it != known_picture_buffers_.end()) {
    *service_texture_id = it->second;
    return true;
  }

  if (!MakeDecoderContextCurrent(command_buffer_->AsWeakPtr()))
    return false;

  gpu::gles2::GLES2Decoder* command_decoder = command_buffer_->decoder();
  gpu::gles2::TextureManager* texture_manager =
      command_decoder->GetContextGroup()->texture_manager();

  gpu::gles2::TextureRef* texture_ref =
      texture_manager->GetTexture(client_texture_id);
  if (!texture_ref) {
    DLOG(ERROR) << "Failed to find texture ID";
    return false;
  }

  gpu::gles2::Texture* info = texture_ref->texture();
  if (info->target() != media::kPlatformMediaPipelineTextureTarget) {
    DLOG(ERROR) << "Texture target mismatch";
    return false;
  }

  texture_manager->SetLevelInfo(
      texture_ref, media::kPlatformMediaPipelineTextureTarget, 0,
      media::kPlatformMediaPipelineTextureFormat,
      video_config_.coded_size.width(), video_config_.coded_size.height(), 1, 0,
      media::kPlatformMediaPipelineTextureFormat, 0, gfx::Rect());

  if (!command_decoder->GetServiceTextureId(client_texture_id,
                                            service_texture_id)) {
    DLOG(ERROR) << "Failed to translate texture ID";
    return false;
  }

  known_picture_buffers_[client_texture_id] = *service_texture_id;
  return true;
}

}  // namespace content
