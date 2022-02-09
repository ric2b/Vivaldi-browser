// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#include "platform_media/renderer/pipeline/ipc_media_pipeline_host.h"

#include <algorithm>
#include <map>
#include <queue>

#include "base/callback_helpers.h"
#include "base/numerics/safe_conversions.h"
#include "base/stl_util.h"
#include "base/trace_event/trace_event.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/data_source.h"
#include "media/base/decoder_buffer.h"
#include "mojo/public/cpp/base/shared_memory_utils.h"

#include "platform_media/common/feature_toggles.h"
#include "platform_media/common/platform_ipc_util.h"
#include "platform_media/common/platform_logging_util.h"
#include "platform_media/common/platform_media_pipeline_types.h"
#include "platform_media/renderer/decoders/ipc_factory.h"

namespace media {

// This can became false after the remote pipeline reports absence of system
// support.
bool IPCMediaPipelineHost::g_available = true;

namespace {

constexpr const char* GetDecodeDataReadTraceEventName(PlatformStreamType type) {
  switch (type) {
    case PlatformStreamType::kAudio:
      return "ReadAudioData";
    case PlatformStreamType::kVideo:
      return "ReadVideoData";
  }
}

template <typename ConfigType>
void HandleConfigInit(const ConfigType& new_config,
                      ConfigType& current_config) {
  if (new_config.is_valid()) {
    VLOG(3) << " PROPMEDIA(RENDERER) : " << __FUNCTION__ << " "
            << GetStreamTypeName(ConfigType::kStreamType)
            << " Config Acceptable : " << Loggable(new_config);
    current_config = new_config;
  } else {
    VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__ << " "
            << GetStreamTypeName(ConfigType::kStreamType)
            << " Config is not valid";
  }
}

template <typename ConfigType>
DemuxerStream::Status HandleConfigChange(PlatformStreamType stream_type,
                                         const ConfigType& new_config,
                                         ConfigType& current_config) {
  if (stream_type != ConfigType::kStreamType) {
    LOG(ERROR) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
               << " Config type mismatchi, expected="
               << GetStreamTypeName(ConfigType::kStreamType)
               << " actual=" << GetStreamTypeName(stream_type);
    return DemuxerStream::kError;
  }
  if (!new_config.is_valid()) {
    LOG(ERROR) << " PROPMEDIA(RENDERER) : " << __FUNCTION__ << " Invalid "
               << GetStreamTypeName(ConfigType::kStreamType)
               << " configuration received";
    return DemuxerStream::kError;
  }

  VLOG(2) << " PROPMEDIA(RENDERER) : " << __FUNCTION__ << " "
          << GetStreamTypeName(ConfigType::kStreamType)
          << " Config change : " << Loggable(new_config);

  current_config = new_config;
  return DemuxerStream::kConfigChanged;
}

}  // namespace

using media::DemuxerStream;

IPCMediaPipelineHost::IPCMediaPipelineHost() {
  VLOG(2) << " PROPMEDIA(RENDERER) : " << __FUNCTION__ << " this=" << this;
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

IPCMediaPipelineHost::~IPCMediaPipelineHost() {
  VLOG(2) << " PROPMEDIA(RENDERER) : " << __FUNCTION__ << " this=" << this
          << " will_stop_remote=" << !!remote_pipeline_;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (remote_pipeline_) {
    TRACE_EVENT0("IPC_MEDIA", "Stop");
    remote_pipeline_->Stop();
  }
}

void IPCMediaPipelineHost::DisconnectHandler() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  remote_pipeline_.reset();
  receiver_.reset();

  // Call any pending callbacks.
  if (init_callback_) {
    // The init callback may delete this, so return immediately. Other callbacks
    // cannot be set at this point.
    DCHECK(!seek_callback_);
    if (DCHECK_IS_ON()) {
      for (auto& callback : decoded_data_read_callbacks_) {
        DCHECK(!callback);
      }
    }
    std::move(init_callback_).Run(false);
    return;
  }
  if (seek_callback_) {
    std::move(seek_callback_).Run(PipelineStatus::PIPELINE_ERROR_ABORT);
  }
  for (auto& callback : decoded_data_read_callbacks_) {
    if (callback) {
      std::move(callback).Run(DemuxerStream::kError, nullptr);
    }
  }
}

namespace {

void InitializeFromConnectorRunner(
    platform_media::mojom::PipelineParamsPtr params,
    platform_media::mojom::PipelineFactory::StartNewPipelineCallback callback,
    platform_media::mojom::PipelineFactory& factory) {
  factory.StartNewPipeline(std::move(params), std::move(callback));
}

}  // namespace

void IPCMediaPipelineHost::Initialize(DataSource* data_source,
                                      std::string mimetype,
                                      InitializeCB callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!remote_pipeline_);
  DCHECK(!receiver_.is_bound());
  DCHECK(data_source);
  DCHECK(callback);
  DCHECK(init_callback_.is_null());

  data_source_ = data_source;
  init_callback_ = std::move(callback);
  int64_t size = -1;
  if (!data_source_->GetSize(&size)) {
    size = -1;
  }

  VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__ << " size=" << size
          << " mimetype=" << mimetype;

  base::MappedReadOnlyRegion region =
      base::ReadOnlySharedMemoryRegion::Create(kIPCSourceSharedMemorySize);
  if (!region.IsValid()) {
    LOG(ERROR) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
               << " allocation failed for size " << kIPCSourceSharedMemorySize;
    std::move(callback).Run(false);
    return;
  }
  raw_mapping_ = std::move(region.mapping);

  mojo::PendingRemote<platform_media::mojom::PipelineDataSource>
      remote_for_gpu = receiver_.BindNewPipeAndPassRemote();

  mojo::PendingReceiver<platform_media::mojom::Pipeline> receiver_for_gpu =
      remote_pipeline_.BindNewPipeAndPassReceiver();

  // Unretained() is safe as this owns the receiver.
  receiver_.set_disconnect_handler(base::BindOnce(
      &IPCMediaPipelineHost::DisconnectHandler, base::Unretained(this)));
  remote_pipeline_.set_disconnect_handler(base::BindOnce(
      &IPCMediaPipelineHost::DisconnectHandler, base::Unretained(this)));

  auto params = platform_media::mojom::PipelineParams::New();
  params->data_source_size = size;
  params->is_data_source_streaming = data_source_->IsStreaming();
  params->mime_type = std::move(mimetype);
  params->data_source_buffer = std::move(region.region);
  params->data_source = std::move(remote_for_gpu);
  params->pipeline = std::move(receiver_for_gpu);

  IPCFactory::GetPipelienFactory(base::BindOnce(
      &InitializeFromConnectorRunner, std::move(params),
      BindToCurrentLoop(base::BindOnce(&IPCMediaPipelineHost::OnInitialized,
                                       weak_ptr_factory_.GetWeakPtr()))));
}

void IPCMediaPipelineHost::OnInitialized(
    platform_media::mojom::PipelineInitResultPtr result) {
  VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
          << " success=" << result->success << " bitrate=" << result->bitrate;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(init_callback_);

  if (result->not_available) {
    g_available = false;
  }

  HandleConfigInit(result->audio_config, audio_config_);
  HandleConfigInit(result->video_config, video_config_);

  bitrate_ = result->bitrate;
  time_info_ = result->time_info;

  // The init callback may delete this.
  std::move(init_callback_).Run(result->success);
}

void IPCMediaPipelineHost::StartWaitingForSeek() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (remote_pipeline_) {
    remote_pipeline_->WillSeek();
  }
}

void IPCMediaPipelineHost::Seek(base::TimeDelta time,
                                PipelineStatusCallback seek_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!seek_callback_);
  if (!remote_pipeline_) {
    std::move(seek_callback).Run(PipelineStatus::PIPELINE_ERROR_ABORT);
    return;
  }

  VLOG(3) << " PROPMEDIA(RENDERER) : " << __FUNCTION__ << " time=" << time;

  TRACE_EVENT_ASYNC_BEGIN0("IPC_MEDIA", "Seek", this);

  // Store the callback in the instance, do not pass it as an argument to bind
  // as me must call it on disconnect.
  seek_callback_ = std::move(seek_callback);

  // Unretained() is safe as this owns the remote.
  remote_pipeline_->Seek(time, base::BindOnce(&IPCMediaPipelineHost::OnSeekDone,
                                              base::Unretained(this)));
}

void IPCMediaPipelineHost::OnSeekDone(bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(seek_callback_);

  VLOG(3) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
          << " success=" << success;

  std::move(seek_callback_)
      .Run(success ? PipelineStatus::PIPELINE_OK
                   : PipelineStatus::PIPELINE_ERROR_ABORT);

  TRACE_EVENT_ASYNC_END0("IPC_MEDIA", "Seek", this);
}

void IPCMediaPipelineHost::ReadDecodedData(PlatformStreamType stream_type,
                                           DemuxerStream::ReadCB read_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!GetElem(decoded_data_read_callbacks_, stream_type))
      << "Overlapping reads are not supported";

  TRACE_EVENT_ASYNC_BEGIN0("IPC_MEDIA",
                           GetDecodeDataReadTraceEventName(stream_type), this);

  if (!remote_pipeline_) {
    std::move(read_cb).Run(DemuxerStream::kAborted, nullptr);
    return;
  }

  // Store the callback in the instance, do not pass it as an argument to bind
  // as me must call it on disconnect.
  GetElem(decoded_data_read_callbacks_, stream_type) = std::move(read_cb);

  // Unretained() is safe as this owns the remote.
  remote_pipeline_->ReadDecodedData(
      stream_type, base::BindOnce(&IPCMediaPipelineHost::OnDecodedDataReady,
                                  base::Unretained(this), stream_type));
}

void IPCMediaPipelineHost::OnDecodedDataReady(
    PlatformStreamType stream_type,
    platform_media::mojom::DecodingResultPtr result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(GetElem(decoded_data_read_callbacks_, stream_type));

  DemuxerStream::Status status = DemuxerStream::kError;
  scoped_refptr<DecoderBuffer> buffer;
  if (result) {
    using Tag = platform_media::mojom::DecodingResult::Tag;
    switch (result->which()) {
      case Tag::END_OF_FILE:
        buffer = DecoderBuffer::CreateEOSBuffer();
        status = DemuxerStream::kOk;
        break;
      case Tag::AUDIO_CONFIG:
        status = HandleConfigChange(stream_type, result->get_audio_config(),
                                    audio_config_);
        break;
      case Tag::VIDEO_CONFIG:
        status = HandleConfigChange(stream_type, result->get_video_config(),
                                    video_config_);
        break;
      case Tag::DECODED_DATA: {
        platform_media::mojom::DecodedDataPtr data =
            std::move(result->get_decoded_data());
        if (data->region.IsValid()) {
          VLOG(5) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
                  << " new decoding region size=" << data->region.GetSize()
                  << " stream_type=" << GetStreamTypeName(stream_type);
          // Release the old cached mapping before allocating new one
          GetElem(decoded_mappings_, stream_type) =
              base::ReadOnlySharedMemoryMapping();
          GetElem(decoded_mappings_, stream_type) = data->region.Map();
          // We no longer need the region.
          data->region = base::ReadOnlySharedMemoryRegion();
          if (!GetElem(decoded_mappings_, stream_type).IsValid()) {
            LOG(ERROR) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
                       << " Failed to map " << data->region.GetSize();
            break;
          }
        }
        const uint8_t* decoded_memory = nullptr;
        size_t decoded_size = 0;
        if (data->size > 0) {
          decoded_size = static_cast<size_t>(data->size);
          if (!GetElem(decoded_mappings_, stream_type).IsValid() ||
              decoded_size > GetElem(decoded_mappings_, stream_type).size()) {
            LOG(ERROR) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
                       << " Inavlid decoding size " << data->size;
            break;
          }
          decoded_memory =
              GetElem(decoded_mappings_, stream_type).GetMemoryAs<uint8_t>();
        }
        buffer = DecoderBuffer::CopyFrom(decoded_memory, decoded_size);
        buffer->set_timestamp(data->timestamp);
        buffer->set_duration(data->duration);
        status = DemuxerStream::kOk;
        break;
      }
    }
  }

  std::move(GetElem(decoded_data_read_callbacks_, stream_type))
      .Run(status, std::move(buffer));
  TRACE_EVENT_ASYNC_END0("IPC_MEDIA",
                         GetDecodeDataReadTraceEventName(stream_type), this);
}

void IPCMediaPipelineHost::ReadRawData(int64_t position,
                                       int32_t size,
                                       ReadRawDataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  TRACE_EVENT_ASYNC_BEGIN0("IPC_MEDIA", "ReadRawData", this);

  // Both receiver_ and remote_pipeline_ are either bound together or
  // disconnected. As we are called on the receiver, it must be valid. And so is
  // the remote.
  DCHECK(receiver_.is_bound());
  DCHECK(remote_pipeline_);
  do {
    if (!data_source_ || !raw_mapping_.IsValid()) {
      LOG(ERROR) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
                 << " unexpected call";
      break;
    }
    if (reading_raw_data_) {
      LOG(ERROR) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
                 << " Call while another read is in process";
      break;
    }
    if (size < 1 || static_cast<size_t>(size) > raw_mapping_.size()) {
      LOG(ERROR) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
                 << " invalid size - " << size;
      break;
    }

    // The Read call assumes that the memory pointer passed to it is valid
    // until it calls the callback. We pass this as a weak pointer. Hence it can
    // be deleted before the callback is called. As such it cannot own the
    // memory. To address this we move the mapping into the callback as a bound
    // argument and then move it back into this when the callback runs.
    base::WritableSharedMemoryMapping mapping = std::move(raw_mapping_);
    uint8_t* raw_memory = mapping.GetMemoryAs<uint8_t>();
    reading_raw_data_ = true;
    data_source_->Read(position, size, raw_memory,
                       BindToCurrentLoop(base::BindOnce(
                           &IPCMediaPipelineHost::OnReadRawDataFinished,
                           weak_ptr_factory_.GetWeakPtr(), std::move(mapping),
                           std::move(callback))));
    return;
  } while (false);

  std::move(callback).Run(DataSource::kReadError);
  TRACE_EVENT_ASYNC_END0("IPC_MEDIA", "ReadRawData", this);
}

void IPCMediaPipelineHost::OnReadRawDataFinished(
    base::WritableSharedMemoryMapping mapping,
    ReadRawDataCallback callback,
    int size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(reading_raw_data_);

  // A negative size indicates a read error.
  VLOG(6) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
          << " raw_data_read_size=" << size;
  reading_raw_data_ = false;

  // See comments in ReadRawData
  raw_mapping_ = std::move(mapping);

  std::move(callback).Run(size);

  TRACE_EVENT_ASYNC_END0("IPC_MEDIA", "ReadRawData", this);
}

}  // namespace media
