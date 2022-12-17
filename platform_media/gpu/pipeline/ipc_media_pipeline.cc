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
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/data_buffer.h"
#include "mojo/public/cpp/base/shared_memory_utils.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

#include "platform_media/common/platform_ipc_util.h"
#include "platform_media/common/platform_logging_util.h"
#include "platform_media/common/platform_media_pipeline_types.h"
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

class IPCMediaPipeline::Factory
    : public platform_media::mojom::PipelineFactory {
 public:
  Factory() {
    VLOG(2) << " PROPMEDIA(GPU) : " << __FUNCTION__ << " this=" << this;
  }

  ~Factory() override {
    VLOG(2) << " PROPMEDIA(GPU) : " << __FUNCTION__ << " this=" << this;
  }

 private:
  friend IPCMediaPipeline;

  void StartNewPipeline(platform_media::mojom::PipelineParamsPtr params,
                        StartNewPipelineCallback callback) override {
    // Pipeline owns itself and self-delete when renderer closes the message
    // pipes or calls Stop().
    IPCMediaPipeline* pipeline = new IPCMediaPipeline();
    pipeline->Initialize(std::move(params), std::move(callback));
  }
};

/* static */
void IPCMediaPipeline::CreateFactory(mojo::GenericPendingReceiver receiver) {
  mojo::PendingReceiver<platform_media::mojom::PipelineFactory>
      factory_receiver = receiver.As<platform_media::mojom::PipelineFactory>();
  if (!factory_receiver) {
    LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__
               << " wrong factory interface - "
               << receiver.interface_name().value_or("");
    return;
  }

  // The factory lives until the renderer process disconnects.
  auto factory = std::make_unique<Factory>();
  mojo::MakeSelfOwnedReceiver(std::move(factory), std::move(factory_receiver));
}

IPCMediaPipeline::IPCMediaPipeline() {
  std::fill(std::begin(has_media_type_), std::end(has_media_type_), false);
  for (PlatformStreamType stream_type : AllStreamTypes()) {
    GetElem(ipc_decoding_buffers_, stream_type).Init(stream_type);
  }
}

IPCMediaPipeline::~IPCMediaPipeline() {
  VLOG(2) << " PROPMEDIA(GPU) : " << __FUNCTION__ << " this=" << this;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (pending_source_buffer_) {
    // In case of abrupt termination like after the renderer process crash the
    // source buffer here may still have a pending callback. Ensure that the
    // callback is called to release of system resources.
    ipc_data_source::Buffer::OnRawDataError(std::move(pending_source_buffer_));
  }
}

void IPCMediaPipeline::DisconnectHandler() {
  VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__ << " this=" << this;
  Stop();
}

void IPCMediaPipeline::Stop() {
  VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__ << " this=" << this;
  data_source_.reset();
  receiver_.reset();
  if (pending_source_buffer_) {
    ipc_data_source::Buffer::OnRawDataError(std::move(pending_source_buffer_));
  }
  base::SequencedTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
}

void IPCMediaPipeline::Initialize(
    platform_media::mojom::PipelineParamsPtr params,
    StartNewPipelineCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(state_, CONSTRUCTED);
  DCHECK(!receiver_.is_bound());
  DCHECK(!data_source_);
  VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__ << " this=" << this
          << " data_size=" << params->data_source_size
          << " streaming=" << params->is_data_source_streaming
          << " mime_type=" << params->mime_type;

  // Bind now to always tell the host about any errors so it can promptly stop.
  data_source_.Bind(std::move(params->data_source));
  receiver_.Bind(std::move(params->pipeline));

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

    // Unretained() is safe as impl_ owns the remote and the disconnect handler
    // is never called after the remote destruction.
    data_source_.set_disconnect_handler(base::BindOnce(
        &IPCMediaPipeline::DisconnectHandler, base::Unretained(this)));
    receiver_.set_disconnect_handler(base::BindOnce(
        &IPCMediaPipeline::DisconnectHandler, base::Unretained(this)));

    ipc_data_source::Info source_info;
    source_info.is_streaming = params->is_data_source_streaming;
    source_info.size = params->data_source_size;
    source_info.mime_type = std::move(params->mime_type);

    source_info.buffer.Init(
        std::move(data_source_mapping),
        base::BindRepeating(&IPCMediaPipeline::ReadRawData,
                            weak_ptr_factory_.GetWeakPtr()));

    state_ = BUSY;
    media_pipeline_->Initialize(
        std::move(source_info),
        base::BindOnce(&IPCMediaPipeline::Initialized,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
    return;
  } while (false);

  std::move(callback).Run(platform_media::mojom::PipelineInitResult::New());
}

/* static */
void IPCMediaPipeline::Initialized(
    base::WeakPtr<IPCMediaPipeline> pipeline,
    StartNewPipelineCallback callback,
    platform_media::mojom::PipelineInitResultPtr result) {
  VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
          << " success=" << result->success
          << " duration=" << result->time_info.duration
          << " bitrate=" << result->bitrate
          << " audio=" << result->audio_config.is_valid()
          << " video=" << result->video_config.is_valid()
          << " pipeline=" << pipeline.get();
  if (!pipeline) {
    // Reset the result to tell the factory caller that the connection to the
    // pipelne instace was closed during initialization.
    result = platform_media::mojom::PipelineInitResult::New();
  } else {
    DCHECK_CALLED_ON_VALID_SEQUENCE(pipeline->sequence_checker_);
    DCHECK_EQ(pipeline->state_, BUSY);

    GetElem(pipeline->has_media_type_, PlatformStreamType::kAudio) =
        result->audio_config.is_valid();
    GetElem(pipeline->has_media_type_, PlatformStreamType::kVideo) =
        result->video_config.is_valid();

    pipeline->state_ = result->success ? DECODING : STOPPED;
  }

  // Always call the callback even when !pipeline.
  std::move(callback).Run(std::move(result));
}

/* static */
void IPCMediaPipeline::ReadRawData(base::WeakPtr<IPCMediaPipeline> pipeline,
                                   ipc_data_source::Buffer buffer) {
  DCHECK(buffer);
  VLOG(4) << " PROPMEDIA(GPU) : " << __FUNCTION__
          << " size=" << buffer.GetRequestedSize()
          << " position=" << buffer.GetReadPosition()
          << " has_pipeline=" << !!pipeline
          << " stopped=" << !pipeline->data_source_.is_bound();
  do {
    if (!pipeline)
      break;

    DCHECK_CALLED_ON_VALID_SEQUENCE(pipeline->sequence_checker_);
    if (!pipeline->data_source_)
      break;
    if (pipeline->pending_source_buffer_) {
      LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__
                 << " attempt to read when another request is active";
      break;
    }

    // Unretained is safe as the pipeline owns hosts_.
    pipeline->data_source_->ReadRawData(
        buffer.GetReadPosition(), buffer.GetRequestedSize(),
        base::BindOnce(&IPCMediaPipeline::OnRawDataReady,
                       base::Unretained(pipeline.get())));
    pipeline->pending_source_buffer_ = std::move(buffer);

    return;
  } while (false);

  ipc_data_source::Buffer::OnRawDataError(std::move(buffer));
}

void IPCMediaPipeline::OnRawDataReady(int read_size) {
  if (!pending_source_buffer_) {
    // This should never happen unless the renderer process in a bad state as
    // we never send a new request until we get a reply.
    LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__ << " unexpected reply";
  } else if (ipc_data_source::Buffer::OnRawDataRead(
                 read_size, std::move(pending_source_buffer_))) {
    return;
  }
  Stop();
}

void IPCMediaPipeline::ReadDecodedData(PlatformStreamType stream_type,
                                       ReadDecodedDataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("IPC_MEDIA", "IPCMediaPipeline::OnReadDecodedData");
  VLOG(7) << " PROPMEDIA(GPU) : " << __FUNCTION__
          << " stream_type=" << GetStreamTypeName(stream_type);

  // We must be in the decoding state and not already running an asynchronous
  // call to decode data of this type.
  do {
    if (state_ != DECODING || !GetElem(ipc_decoding_buffers_, stream_type)) {
      LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__
                 << " Unexpected call to " << __FUNCTION__;
      break;
    }
    if (!has_media_type(stream_type)) {
      LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__
                 << " No data of given media kind ("
                 << GetStreamTypeName(stream_type) << ") to decode";
      break;
    }

    TRACE_EVENT_ASYNC_BEGIN0(
        "IPC_MEDIA", GetDecodeDataReadTraceEventName(stream_type), this);

    IPCDecodingBuffer buffer =
        std::move(GetElem(ipc_decoding_buffers_, stream_type));
    buffer.set_reply_cb(base::BindOnce(&IPCMediaPipeline::DecodedDataReady,
                                       weak_ptr_factory_.GetWeakPtr(),
                                       std::move(callback)));
    media_pipeline_->ReadMediaData(std::move(buffer));
    return;
  } while (false);

  std::move(callback).Run(nullptr);
}

void IPCMediaPipeline::DecodedDataReady(ReadDecodedDataCallback callback,
                                        IPCDecodingBuffer buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(state_, DECODING);

  PlatformStreamType stream_type = buffer.stream_type();
  DCHECK(!GetElem(ipc_decoding_buffers_, stream_type));
  DCHECK(buffer);

  platform_media::mojom::DecodingResultPtr result;
  base::ReadOnlySharedMemoryRegion region_to_send;
  switch (buffer.status()) {
    case MediaDataStatus::kMediaError:
      VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
              << " status : MediaDataStatus::kMediaError";
      result.reset();
      break;
    case MediaDataStatus::kEOS:
      VLOG(2) << " PROPMEDIA(GPU) : " << __FUNCTION__
              << " eos stream_type=" << GetStreamTypeName(stream_type);
      result = platform_media::mojom::DecodingResult::NewEndOfFile(true);
      break;
    case MediaDataStatus::kConfigChanged:
      VLOG(3) << " PROPMEDIA(GPU) : " << __FUNCTION__
              << " config_change stream_type="
              << GetStreamTypeName(stream_type);
      switch (stream_type) {
        case PlatformStreamType::kAudio:
          DCHECK(buffer.GetAudioConfig().is_valid());
          result = platform_media::mojom::DecodingResult::NewAudioConfig(
              buffer.GetAudioConfig());
          break;
        case PlatformStreamType::kVideo:
          DCHECK(buffer.GetVideoConfig().is_valid());
          result = platform_media::mojom::DecodingResult::NewVideoConfig(
              buffer.GetVideoConfig());
          break;
      }
      break;
    case MediaDataStatus::kOk:
      VLOG(7) << " PROPMEDIA(GPU) : " << __FUNCTION__
              << "decoded_data stream_type=" << GetStreamTypeName(stream_type)
              << " status=" << static_cast<int>(buffer.status())
              << " data_size=" << buffer.data_size();

      auto data = platform_media::mojom::DecodedData::New();
      data->size = buffer.data_size();
      data->timestamp = buffer.timestamp();
      data->duration = buffer.duration();
      data->region = buffer.extract_region_to_send();
      result = platform_media::mojom::DecodingResult::NewDecodedData(
          std::move(data));
      break;
  }

  // Reuse the buffer the next time.
  GetElem(ipc_decoding_buffers_, stream_type) = std::move(buffer);

  std::move(callback).Run(std::move(result));

  TRACE_EVENT_ASYNC_END0("IPC_MEDIA",
                         GetDecodeDataReadTraceEventName(stream_type), this);
}

void IPCMediaPipeline::WillSeek() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(receiver_.is_bound());
  VLOG(3) << " PROPMEDIA(GPU) : " << __FUNCTION__ << " reading_audio="
          << !GetElem(ipc_decoding_buffers_, PlatformStreamType::kAudio)
          << " reading_video="
          << !GetElem(ipc_decoding_buffers_, PlatformStreamType::kVideo);

  media_pipeline_->WillSeek();
}

void IPCMediaPipeline::Seek(base::TimeDelta time, SeekCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(3) << " PROPMEDIA(GPU) : " << __FUNCTION__ << " reading_audio="
          << !GetElem(ipc_decoding_buffers_, PlatformStreamType::kAudio)
          << " reading_video="
          << !GetElem(ipc_decoding_buffers_, PlatformStreamType::kVideo);

  if (state_ != DECODING) {
    LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__ << " Unexpected call to "
               << __FUNCTION__;
    std::move(callback).Run(false);
    return;
  }
  state_ = BUSY;

  media_pipeline_->Seek(time, base::BindOnce(&IPCMediaPipeline::SeekDone,
                                             weak_ptr_factory_.GetWeakPtr(),
                                             std::move(callback)));
}

void IPCMediaPipeline::SeekDone(SeekCallback callback, bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(state_, BUSY);

  state_ = DECODING;
  std::move(callback).Run(success);
}

}  // namespace media
