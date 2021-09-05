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
#include "base/stl_util.h"
#include "base/trace_event/trace_event.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/data_source.h"
#include "media/base/decoder_buffer.h"
#include "mojo/public/cpp/base/shared_memory_utils.h"

#include "platform_media/common/feature_toggles.h"
#include "platform_media/common/media_pipeline_messages.h"
#include "platform_media/common/platform_ipc_util.h"
#include "platform_media/common/platform_logging_util.h"
#include "platform_media/common/platform_media_pipeline_types.h"
#include "platform_media/common/video_frame_transformer.h"
#include "platform_media/renderer/decoders/ipc_factory.h"

namespace media {

namespace {

const char* const kDecodedDataReadTraceEventNames[] = {"ReadAudioData",
                                                       "ReadVideoData"};
static_assert(base::size(kDecodedDataReadTraceEventNames) ==
                  kPlatformMediaDataTypeCount,
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

IPCMediaPipelineHostImpl::IPCMediaPipelineHostImpl(gpu::GpuChannelHost* channel)
    : channel_(channel),
      routing_id_(MSG_ROUTING_NONE),
      weak_ptr_factory_(this) {
  DCHECK(channel_);
  LOG(INFO) << " PROPMEDIA(RENDERER) : " << __FUNCTION__;
  DCHECK(IPCFactory::MainTaskRunner()->RunsTasksInCurrentSequence());
}

IPCMediaPipelineHostImpl::~IPCMediaPipelineHostImpl() {
  DCHECK(IPCFactory::MediaTaskRunner()->RunsTasksInCurrentSequence());

  if (is_connected()) {
    TRACE_EVENT0("IPC_MEDIA", "Stop");

    channel_->Send(new MediaPipelineMsg_Destroy(routing_id_));
    channel_->RemoveRoute(routing_id_);
  }
}

void IPCMediaPipelineHostImpl::Initialize(const std::string& mimetype,
                                          InitializeCB callback) {
  DCHECK(IPCFactory::MediaTaskRunner()->RunsTasksInCurrentSequence());
  DCHECK(!is_connected());
  DCHECK(data_source_);
  DCHECK(callback);
  DCHECK(init_callback_.is_null());

  VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
          << " With Mimetype : " << mimetype;

  routing_id_ = channel_->GenerateRouteID();

  VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
          << " Create pipeline";

  if (!channel_->Send(new MediaPipelineMsg_New(routing_id_))) {
    std::move(callback).Run(false);
    return;
  }

  channel_->AddRoute(routing_id_, weak_ptr_factory_.GetWeakPtr());

  init_callback_ = std::move(callback);
  int64_t size = -1;
  if (!data_source_->GetSize(&size))
    size = -1;

  VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
          << " Initialize pipeline - size of source : " << size;

  channel_->Send(new MediaPipelineMsg_Initialize(
      routing_id_, size, data_source_->IsStreaming(), mimetype));
}

void IPCMediaPipelineHostImpl::OnInitialized(
    bool success,
    int bitrate,
    const PlatformMediaTimeInfo& time_info,
    const PlatformAudioConfig& audio_config,
    const PlatformVideoConfig& video_config) {
  DCHECK(IPCFactory::MediaTaskRunner()->RunsTasksInCurrentSequence());

  if (!init_callback_) {
    LOG(ERROR) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
               << " Unexpected MediaPipelineMsg_Initialized";
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
  bitrate_ = bitrate;
  time_info_ = time_info;

  std::move(init_callback_).Run(success);
}

void IPCMediaPipelineHostImpl::StartWaitingForSeek() {
  DCHECK(IPCFactory::MediaTaskRunner()->RunsTasksInCurrentSequence());
  // This seek hint can be called at any moment.
  if (is_connected()) {
    channel_->Send(new MediaPipelineMsg_WillSeek(routing_id_));
  }
}

void IPCMediaPipelineHostImpl::Seek(base::TimeDelta time,
                                    PipelineStatusCallback status_cb) {
  DCHECK(IPCFactory::MediaTaskRunner()->RunsTasksInCurrentSequence());
  DCHECK(is_connected());
  DCHECK(!seek_callback_);

  TRACE_EVENT_ASYNC_BEGIN0("IPC_MEDIA", "Seek", this);

  seek_callback_ = std::move(status_cb);
  channel_->Send(new MediaPipelineMsg_Seek(routing_id_, time));
}

void IPCMediaPipelineHostImpl::OnSought(bool success) {
  DCHECK(IPCFactory::MediaTaskRunner()->RunsTasksInCurrentSequence());

  if (!seek_callback_) {
    LOG(ERROR) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
               << " Unexpected MediaPipelineMsg_Sought";
    return;
  }

  LOG_IF(WARNING, !success)
      << " PROPMEDIA(RENDERER) : " << __FUNCTION__
      << " PIPELINE_ERROR_ABORT";

  std::move(seek_callback_)
      .Run(success ? PipelineStatus::PIPELINE_OK
                   : PipelineStatus::PIPELINE_ERROR_ABORT);

  TRACE_EVENT_ASYNC_END0("IPC_MEDIA", "Seek", this);
}

void IPCMediaPipelineHostImpl::ReadDecodedData(
    PlatformMediaDataType type,
    DemuxerStream::ReadCB read_cb) {
  DCHECK(IPCFactory::MediaTaskRunner()->RunsTasksInCurrentSequence());
  DCHECK(!is_read_in_progress(type)) << "Overlapping reads are not supported";
  DCHECK(is_connected());

  TRACE_EVENT_ASYNC_BEGIN0(
      "IPC_MEDIA", kDecodedDataReadTraceEventNames[type], this);

  if (!channel_->Send(
          new MediaPipelineMsg_ReadDecodedData(routing_id_, type))) {
    std::move(read_cb).Run(DemuxerStream::kAborted, nullptr);
    return;
  }

  decoded_data_read_callbacks_[type] = std::move(read_cb);
}

void IPCMediaPipelineHostImpl::OnReadRawData(
    int64_t tag,
    int64_t position,
    int size) {
  DCHECK(IPCFactory::MediaTaskRunner()->RunsTasksInCurrentSequence());

  TRACE_EVENT_ASYNC_BEGIN0("IPC_MEDIA", "ReadRawData", this);

  if (reading_raw_data_) {
    LOG(ERROR) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
               << " Unexpected MediaPipelineMsg_ReadRawData";
    return;
  }
  do {
    // TODO(igor@vivaldi.com): Figure out if size can be zero here.
    if (!(0 <= size && size <= kMaxSharedMemorySize)) {
      LOG(ERROR) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
                 << " invalid size - " << size;
      break;
    }
    if (!data_source_) {
      LOG(ERROR) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
                 << " unexpected call";
      break;
    }
    // A default invalid region tells to reuse the old one.
    base::ReadOnlySharedMemoryRegion raw_region_to_send;
    if (size > 0 && (!raw_mapping_.IsValid() ||
                     static_cast<size_t>(size) > raw_mapping_.size())) {
      size_t region_size =
          RoundUpTo4kPage(std::max(kMinimalSharedMemorySize, size));
      VLOG(5) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
              << " size of allocation " << region_size;
      // Release the old memory before we try to get new one.
      raw_mapping_ = base::WritableSharedMemoryMapping();
      base::MappedReadOnlyRegion region =
          base::ReadOnlySharedMemoryRegion::Create(region_size);
      if (!region.IsValid()) {
        LOG(ERROR) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
                   << " allocation failed for size " << region_size;
        break;
      }
      raw_region_to_send = std::move(region.region);
      raw_mapping_ = std::move(region.mapping);
    }

    // The Read call assumes that the memory pointer passed to it is valid
    // until it calls the callback. We pass this as a weak pointer. Hence it can
    // be deleted before the callback is called. As such it cannot own the
    // memory. To address this we move the mapping into the callback as a bound
    // argument and then move it back into this when the callback runs.
    //
    // As we embedd move-only instances into the callback, we need to use
    // AdaptCallbackForRepeating as DataSource takes repeating callback.
    base::WritableSharedMemoryMapping mapping = std::move(raw_mapping_);
    uint8_t* raw_memory = mapping.GetMemoryAs<uint8_t>();
    reading_raw_data_ = true;
    data_source_->Read(
        position, size, raw_memory,
        AdaptCallbackForRepeating(BindToCurrentLoop(
            base::BindOnce(&IPCMediaPipelineHostImpl::OnReadRawDataFinished,
                       weak_ptr_factory_.GetWeakPtr(), tag,
                       std::move(raw_region_to_send), std::move(mapping)))));
    return;
  } while (false);

  channel_->Send(new MediaPipelineMsg_RawDataReady(
      routing_id_, tag, DataSource::kReadError,
      base::ReadOnlySharedMemoryRegion()));
  TRACE_EVENT_ASYNC_END0("IPC_MEDIA", "ReadRawData", this);
}

void IPCMediaPipelineHostImpl::OnReadRawDataFinished(
    int64_t tag,
    base::ReadOnlySharedMemoryRegion raw_region_to_send,
    base::WritableSharedMemoryMapping mapping,
    int size) {
  // Negative size indicates a read error.
  DCHECK(IPCFactory::MediaTaskRunner()->RunsTasksInCurrentSequence());
  DCHECK(reading_raw_data_);
  reading_raw_data_ = false;

  if (!is_connected())
    // Someone called Stop() after we got the ReadRawData message and beat us
    // to it.
    return;

  // See comments in OnReadRawData
  raw_mapping_ = std::move(mapping);

  // If raw_region_to_send_ is not valid, it means to reuse the old region.
  channel_->Send(new MediaPipelineMsg_RawDataReady(
      routing_id_, tag, size, std::move(raw_region_to_send)));
  TRACE_EVENT_ASYNC_END0("IPC_MEDIA", "ReadRawData", this);
}

bool IPCMediaPipelineHostImpl::OnMessageReceived(const IPC::Message& msg) {
  // GpuChannelHost gives us messages for handling on the thread that called
  // AddRoute().
  DCHECK(IPCFactory::MediaTaskRunner()->RunsTasksInCurrentSequence());

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(IPCMediaPipelineHostImpl, msg)
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
  DCHECK(IPCFactory::MediaTaskRunner()->RunsTasksInCurrentSequence());
  return routing_id_ != MSG_ROUTING_NONE;
}

void IPCMediaPipelineHostImpl::OnDecodedDataReady(
    const MediaPipelineMsg_DecodedDataReady_Params& params,
    base::ReadOnlySharedMemoryRegion region) {
  DCHECK(IPCFactory::MediaTaskRunner()->RunsTasksInCurrentSequence());
  DCHECK(!region.IsValid() ||
         (params.status == MediaDataStatus::kOk && params.size > 0));

  if (!is_read_in_progress(params.type)) {
    LOG(ERROR) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
               << " Unexpected MediaPipelineMsg_DecodedDataReady";
    return;
  }

  bool decoding_error = false;
  switch (params.status) {
    case MediaDataStatus::kOk: {
      if (region.IsValid()) {
        VLOG(5) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
                << " new decoding region size=" << region.GetSize()
                << " type=" << params.type;
        // Release the old cached mapping before allocating new one
        decoded_mappings_[params.type] = base::ReadOnlySharedMemoryMapping();
        decoded_mappings_[params.type] = region.Map();
        // We no longer need the region.
        region = base::ReadOnlySharedMemoryRegion();
        if (!decoded_mappings_[params.type].IsValid()) {
          LOG(ERROR) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
                     << " Failed to map " << region.GetSize();
          decoding_error = true;
          break;
        }
      }
      const uint8_t* decoded_memory = nullptr;
      size_t decoded_size = 0;
      if (params.size > 0) {
        decoded_size = static_cast<size_t>(params.size);
        if (!decoded_mappings_[params.type].IsValid() ||
            decoded_size > decoded_mappings_[params.type].size()) {
          LOG(ERROR) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
                     << " Inavlid decoding size " << params.size;
          decoding_error = true;
          break;
        }
        decoded_memory = decoded_mappings_[params.type].GetMemoryAs<uint8_t>();
      }
      scoped_refptr<DecoderBuffer> buffer =
          DecoderBuffer::CopyFrom(decoded_memory, decoded_size);
      buffer->set_timestamp(params.timestamp);
      buffer->set_duration(params.duration);

      std::move(decoded_data_read_callbacks_[params.type])
          .Run(DemuxerStream::kOk, buffer);
      break;
    }

    case MediaDataStatus::kEOS:
      std::move(decoded_data_read_callbacks_[params.type])
          .Run(DemuxerStream::kOk, DecoderBuffer::CreateEOSBuffer());
      break;

    case MediaDataStatus::kConfigChanged:
      std::move(decoded_data_read_callbacks_[params.type])
          .Run(DemuxerStream::kConfigChanged, nullptr);
      break;

    case MediaDataStatus::kMediaError:
      decoding_error = true;
      break;

    default:
      NOTREACHED();
      break;
  }

  if (decoding_error) {
    // Note that this is a decoder error rather than demuxer error.  Don't
    // return DemuxerStream::kAborted.  Instead, return an empty buffer so
    // that the decoder can signal a decoder error.
    std::move(decoded_data_read_callbacks_[params.type])
        .Run(DemuxerStream::kOk, new DecoderBuffer(0));
  }
  TRACE_EVENT_ASYNC_END0("IPC_MEDIA",
                         kDecodedDataReadTraceEventNames[params.type], this);
}

void IPCMediaPipelineHostImpl::OnAudioConfigChanged(
    const PlatformAudioConfig& new_audio_config) {
  DCHECK(IPCFactory::MediaTaskRunner()->RunsTasksInCurrentSequence());

  if (!is_read_in_progress(PlatformMediaDataType::PLATFORM_MEDIA_AUDIO)) {
    LOG(ERROR) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
               << " Unexpected MediaPipelineMsg_AudioConfigChanged";
    return;
  }

  VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
          << " Previous Config "
          << Loggable(audio_config_);
  VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
          << " New Config "
          << Loggable(new_audio_config);

  MediaPipelineMsg_DecodedDataReady_Params params;
  HandleConfigChange<PlatformMediaDataType::PLATFORM_MEDIA_AUDIO>(
      new_audio_config, &audio_config_, &params);
  OnDecodedDataReady(params);
}

void IPCMediaPipelineHostImpl::OnVideoConfigChanged(
    const PlatformVideoConfig& new_video_config) {
  DCHECK(IPCFactory::MediaTaskRunner()->RunsTasksInCurrentSequence());

  if (!is_read_in_progress(PlatformMediaDataType::PLATFORM_MEDIA_VIDEO)) {
    LOG(ERROR) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
               << " Unexpected MediaPipelineMsg_VideoConfigChanged";
    return;
  }

  MediaPipelineMsg_DecodedDataReady_Params params;
  VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
          << " Previous Config "
          << Loggable(video_config_);
  VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
          << " New Config "
          << Loggable(new_video_config);
  HandleConfigChange<PlatformMediaDataType::PLATFORM_MEDIA_VIDEO>(
      new_video_config, &video_config_, &params);

  OnDecodedDataReady(params);
}

}  // namespace media
