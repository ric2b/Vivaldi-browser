// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#include "content/renderer/media/ipc_media_pipeline_host_impl.h"

#include <algorithm>
#include <map>
#include <queue>

#include "base/callback_helpers.h"
#include "base/numerics/safe_conversions.h"
#include "base/trace_event/trace_event.h"
#include "content/common/gpu/media/platform_media_pipeline.h"
#include "content/common/media/media_pipeline_messages.h"
#include "content/renderer/render_thread_impl.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/data_source.h"
#include "media/base/decoder_buffer.h"
#include "media/filters/pass_through_decoder_texture.h"
#include "media/filters/platform_media_pipeline_constants.h"
#include "media/filters/platform_media_pipeline_types.h"
#include "media/renderers/gpu_video_accelerator_factories.h"

namespace {

struct IPCPictureBuffer {
  IPCPictureBuffer(uint32_t texture_id, gpu::Mailbox texture_mailbox)
      : texture_id(texture_id), texture_mailbox(texture_mailbox) {}

  uint32_t texture_id;
  gpu::Mailbox texture_mailbox;
};

const char* const kDecodedDataReadTraceEventNames[] = {"ReadAudioData",
                                                       "ReadVideoData"};
static_assert(arraysize(kDecodedDataReadTraceEventNames) ==
                  media::PLATFORM_MEDIA_DATA_TYPE_COUNT,
              "Incorrect number of defined tracing event names.");

template <media::PlatformMediaDataType type, typename ConfigType>
void HandleConfigChange(const ConfigType& new_config,
                        ConfigType* current_config,
                        MediaPipelineMsg_DecodedDataReady_Params* params) {
  params->type = type;

  if (!new_config.is_valid()) {
    DLOG(ERROR) << "Invalid configuration received";
    params->status = media::kError;
    return;
  }

  *current_config = new_config;
  params->status = media::kConfigChanged;
}

}  // namespace

namespace content {

using media::DemuxerStream;

// Manages IPCPictureBuffers used for transferring video frames which were
// decoded using hardware acceleration.
class IPCMediaPipelineHostImpl::PictureBufferManager {
 public:
  explicit PictureBufferManager(media::GpuVideoAcceleratorFactories* factories);
  ~PictureBufferManager();

  const IPCPictureBuffer* ProvidePictureBuffer(const gfx::Size& size);
  scoped_ptr<media::AutoReleasedPassThroughDecoderTexture> CreateWrappedTexture(
      uint32_t texture_id);
  void DismissPictureBufferInUse();

 private:
  static void ReleaseMailbox(
      base::WeakPtr<PictureBufferManager> buffer_manager,
      media::GpuVideoAcceleratorFactories* factories,
      uint32_t texture_id,
      const gpu::SyncToken& release_sync_point);

  void ReusePictureBuffer(uint32_t texture_id);

  media::GpuVideoAcceleratorFactories* factories_;

  scoped_ptr<IPCPictureBuffer> picture_buffer_in_use_;
  std::queue<IPCPictureBuffer> available_picture_buffers_;
  std::map<int32_t, IPCPictureBuffer> picture_buffers_at_display_;

  base::WeakPtrFactory<PictureBufferManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PictureBufferManager);
};

IPCMediaPipelineHostImpl::PictureBufferManager::PictureBufferManager(
    media::GpuVideoAcceleratorFactories* factories)
    : factories_(factories), weak_ptr_factory_(this) {
  DCHECK(factories_);
}

IPCMediaPipelineHostImpl::PictureBufferManager::~PictureBufferManager() {
  if (picture_buffer_in_use_) {
    factories_->DeleteTexture(picture_buffer_in_use_->texture_id);
  }

  while (!available_picture_buffers_.empty()) {
    factories_->DeleteTexture(available_picture_buffers_.front().texture_id);
    available_picture_buffers_.pop();
  }

  // Textures described by PictureBuffer objects stored in
  // |picture_buffers_at_display_| are in use by an external object which is
  // responsible to properly dispose of them, once they are no longer needed.
}

const IPCPictureBuffer*
IPCMediaPipelineHostImpl::PictureBufferManager::ProvidePictureBuffer(
    const gfx::Size& size) {
  DCHECK(factories_->GetTaskRunner()->RunsTasksOnCurrentThread());
  DCHECK(!picture_buffer_in_use_);

  if (!available_picture_buffers_.empty()) {
    picture_buffer_in_use_.reset(
        new IPCPictureBuffer(available_picture_buffers_.front()));
    available_picture_buffers_.pop();
    return picture_buffer_in_use_.get();
  }

  std::vector<uint32_t> texture_ids;
  std::vector<gpu::Mailbox> texture_mailboxes;
  if (!factories_->CreateTextures(1, size, &texture_ids, &texture_mailboxes,
                                  media::kPlatformMediaPipelineTextureTarget)) {
    DLOG(ERROR) << "Failed to create texture.";
    return NULL;
  }
  DCHECK_EQ(texture_ids.size(), 1U);
  DCHECK_EQ(texture_mailboxes.size(), 1U);

  picture_buffer_in_use_.reset(
      new IPCPictureBuffer(texture_ids[0], texture_mailboxes[0]));
  return picture_buffer_in_use_.get();
}

scoped_ptr<media::AutoReleasedPassThroughDecoderTexture>
IPCMediaPipelineHostImpl::PictureBufferManager::CreateWrappedTexture(
    uint32_t texture_id) {
  DCHECK(picture_buffer_in_use_);

  if (picture_buffer_in_use_->texture_id != texture_id) {
    DLOG(ERROR) << "Unexpected texture id " << texture_id;
    return NULL;
  }

  scoped_ptr<media::PassThroughDecoderTexture> texture_info(
      new media::PassThroughDecoderTexture);
  texture_info->texture_id = texture_id;
  texture_info->mailbox_holder = make_scoped_ptr(new gpu::MailboxHolder(
      picture_buffer_in_use_->texture_mailbox, gpu::SyncToken(),
      media::kPlatformMediaPipelineTextureTarget));
  // This callback has to be run when texture is no longer needed.
  // PassThroughDecoderTextureWrapper will take care of it if no one will try to
  // use its load.
  texture_info->mailbox_holder_release_cb = media::BindToCurrentLoop(
      base::Bind(&ReleaseMailbox, weak_ptr_factory_.GetWeakPtr(), factories_,
                 picture_buffer_in_use_->texture_id));

  picture_buffers_at_display_.insert(
      std::make_pair(texture_id, *picture_buffer_in_use_));
  picture_buffer_in_use_.reset();

  return make_scoped_ptr(
     new media::AutoReleasedPassThroughDecoderTexture(std::move(texture_info)));
}

void IPCMediaPipelineHostImpl::PictureBufferManager::ReleaseMailbox(
    base::WeakPtr<PictureBufferManager> buffer_manager,
    media::GpuVideoAcceleratorFactories* factories,
    uint32_t texture_id,
    const gpu::SyncToken& release_sync_point) {
  DCHECK(factories->GetTaskRunner()->BelongsToCurrentThread());
  factories->WaitSyncToken(release_sync_point);

  if (buffer_manager) {
    buffer_manager->ReusePictureBuffer(texture_id);
    return;
  }

  // This is the last chance to delete this texture if |buffer_manager| exists
  // no more.
  factories->DeleteTexture(texture_id);
}

void IPCMediaPipelineHostImpl::PictureBufferManager::ReusePictureBuffer(
    uint32_t texture_id) {
  auto it = picture_buffers_at_display_.find(texture_id);
  DCHECK(it != picture_buffers_at_display_.end());

  available_picture_buffers_.push(it->second);
  picture_buffers_at_display_.erase(it);
}

void IPCMediaPipelineHostImpl::PictureBufferManager::
    DismissPictureBufferInUse() {
  if (picture_buffer_in_use_) {
    available_picture_buffers_.push(*picture_buffer_in_use_);
    picture_buffer_in_use_.reset();
  }
}

class IPCMediaPipelineHostImpl::SharedData {
 public:
  explicit SharedData(GpuChannelHost* channel);
  ~SharedData();

  bool Prepare(size_t size);
  // Checks if internal buffer is present and big enough.
  bool IsSufficient(int needed_size) const;

  base::SharedMemoryHandle handle() const;
  uint8_t* memory() const;
  int mapped_size() const;

 private:
  GpuChannelHost* channel_;
  scoped_ptr<base::SharedMemory> memory_;
};

IPCMediaPipelineHostImpl::SharedData::SharedData(GpuChannelHost* channel)
    : channel_(channel) {
  DCHECK(channel_);
}

IPCMediaPipelineHostImpl::SharedData::~SharedData() {
}

bool IPCMediaPipelineHostImpl::SharedData::Prepare(size_t size) {
  if (size == 0 || !base::IsValueInRangeForNumericType<int>(size)) {
    return false;
  }

  if (!IsSufficient(static_cast<int>(size))) {
    memory_ = channel_->factory()->AllocateSharedMemory(size);
    if (!memory_)
      return false;

    if (!memory_->Map(size)) {
      memory_.reset(NULL);
      return false;
    }
  }
  return true;
}

bool IPCMediaPipelineHostImpl::SharedData::IsSufficient(int needed_size) const {
  return base::IsValueInRangeForNumericType<size_t>(needed_size) && memory_ &&
         memory_->mapped_size() >= static_cast<size_t>(needed_size);
}

base::SharedMemoryHandle IPCMediaPipelineHostImpl::SharedData::handle() const {
  DCHECK(memory_);
  return memory_->handle();
}

uint8_t* IPCMediaPipelineHostImpl::SharedData::memory() const {
  DCHECK(memory_);
  return static_cast<uint8_t*>(memory_->memory());
}

int IPCMediaPipelineHostImpl::SharedData::mapped_size() const {
  DCHECK(memory_);
  return base::saturated_cast<int>(memory_->mapped_size());
}

IPCMediaPipelineHostImpl::IPCMediaPipelineHostImpl(
    GpuChannelHost* channel,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    media::GpuVideoAcceleratorFactories* factories,
    media::DataSource* data_source)
    : task_runner_(task_runner),
      data_source_(data_source),
      channel_(channel),
      routing_id_(MSG_ROUTING_NONE),
      shared_raw_data_(new SharedData(channel)),
      factories_(factories),
      weak_ptr_factory_(this) {
  DCHECK(channel_.get());
  DCHECK(data_source_);

  for (int i = 0; i < media::PLATFORM_MEDIA_DATA_TYPE_COUNT; ++i) {
    shared_decoded_data_[i].reset(new SharedData(channel));
  }
}

IPCMediaPipelineHostImpl::~IPCMediaPipelineHostImpl() {
  // We use WeakPtrs which require that we (i.e. our |weak_ptr_factory_|) are
  // destroyed on the same thread they are used.
  DCHECK(task_runner_->RunsTasksOnCurrentThread());

  if (is_connected()) {
    DLOG(ERROR) << "Object was not brought down properly. Missing "
                   "MediaPipelineMsg_Stopped?";
  }
}

void IPCMediaPipelineHostImpl::Initialize(const std::string& mimetype,
                                          const InitializeCB& callback) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  DCHECK(!is_connected());
  DCHECK(init_callback_.is_null());

  routing_id_ = channel_->GenerateRouteID();
  int32_t gpu_video_accelerator_factories_route_id =
      factories_ ? factories_->GetRouteID() : MSG_ROUTING_NONE;
  if (!channel_->Send(new MediaPipelineMsg_New(
          routing_id_, gpu_video_accelerator_factories_route_id))) {
    callback.Run(false, -1, media::PlatformMediaTimeInfo(),
                 media::PlatformAudioConfig(), media::PlatformVideoConfig());
    return;
  }

  channel_->AddRoute(routing_id_, weak_ptr_factory_.GetWeakPtr());

  init_callback_ = callback;
  int64_t size = -1;
  if (!data_source_->GetSize(&size))
    size = -1;
  channel_->Send(new MediaPipelineMsg_Initialize(
      routing_id_, size, data_source_->IsStreaming(), mimetype));
}

void IPCMediaPipelineHostImpl::OnInitialized(
    bool success,
    int bitrate,
    const media::PlatformMediaTimeInfo& time_info,
    const media::PlatformAudioConfig& audio_config,
    const media::PlatformVideoConfig& video_config) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());

  if (init_callback_.is_null()) {
    DLOG(ERROR) << "Unexpected call to " << __FUNCTION__;
    return;
  }

  if (audio_config.is_valid()) {
    audio_config_ = audio_config;
  }

  if (video_config.is_valid()) {
    video_config_ = video_config;
    if (video_config_.decoding_mode ==
        media::PlatformMediaDecodingMode::HARDWARE) {
      picture_buffer_manager_.reset(new PictureBufferManager(factories_));
    }
  }

  success = success && bitrate >= 0;

  base::ResetAndReturn(&init_callback_)
      .Run(success, bitrate, time_info, audio_config, video_config);
}

void IPCMediaPipelineHostImpl::OnRequestBufferForRawData(
    size_t requested_size) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());

  if (shared_raw_data_->Prepare(requested_size)) {
    channel_->Send(new MediaPipelineMsg_BufferForRawDataReady(
        routing_id_,
        shared_raw_data_->mapped_size(),
        channel_->ShareToGpuProcess(shared_raw_data_->handle())));
  } else {
    channel_->Send(new MediaPipelineMsg_BufferForRawDataReady(
        routing_id_, 0, base::SharedMemory::NULLHandle()));
  }
}

void IPCMediaPipelineHostImpl::OnRequestBufferForDecodedData(
    media::PlatformMediaDataType type,
    size_t requested_size) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());

  if (!is_read_in_progress(type)) {
    DLOG(ERROR) << "Unexpected call to " << __FUNCTION__;
    return;
  }

  if (shared_decoded_data_[type]->Prepare(requested_size)) {
    channel_->Send(new MediaPipelineMsg_BufferForDecodedDataReady(
        routing_id_,
        type,
        shared_decoded_data_[type]->mapped_size(),
        channel_->ShareToGpuProcess(shared_decoded_data_[type]->handle())));
  } else {
    channel_->Send(new MediaPipelineMsg_BufferForDecodedDataReady(
        routing_id_, type, 0, base::SharedMemory::NULLHandle()));
  }
}

void IPCMediaPipelineHostImpl::StartWaitingForSeek() {
  channel_->Send(new MediaPipelineMsg_WillSeek(routing_id_));
}

void IPCMediaPipelineHostImpl::Seek(base::TimeDelta time,
                                    const media::PipelineStatusCB& status_cb) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  DCHECK(is_connected());
  DCHECK(seek_callback_.is_null());

  TRACE_EVENT_ASYNC_BEGIN0("IPC_MEDIA", "Seek", this);

  seek_callback_ = status_cb;
  channel_->Send(new MediaPipelineMsg_Seek(routing_id_, time));
}

void IPCMediaPipelineHostImpl::OnSought(bool success) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());

  if (seek_callback_.is_null()) {
    DLOG(ERROR) << "Unexpected call to " << __FUNCTION__;
    return;
  }

  base::ResetAndReturn(&seek_callback_)
      .Run(success ? media::PIPELINE_OK : media::PIPELINE_ERROR_ABORT);

  TRACE_EVENT_ASYNC_END0("IPC_MEDIA", "Seek", this);
}

void IPCMediaPipelineHostImpl::Stop() {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  DCHECK(is_connected());

  TRACE_EVENT0("IPC_MEDIA", "Stop");

  channel_->Send(new MediaPipelineMsg_Stop(routing_id_));

  channel_->Send(new MediaPipelineMsg_Destroy(routing_id_));
  channel_->RemoveRoute(routing_id_);
  routing_id_ = MSG_ROUTING_NONE;

  data_source_->Stop();
}

void IPCMediaPipelineHostImpl::ReadDecodedData(
    media::PlatformMediaDataType type,
    const DemuxerStream::ReadCB& read_cb) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  DCHECK(!is_read_in_progress(type)) << "Overlapping reads are not supported";
  DCHECK(is_connected());

  TRACE_EVENT_ASYNC_BEGIN0(
      "IPC_MEDIA", kDecodedDataReadTraceEventNames[type], this);

  uint32_t texture_id = 0;

  if (is_handling_accelerated_video_decode(type)) {
    DCHECK(picture_buffer_manager_);

    const IPCPictureBuffer* picture_buffer =
        picture_buffer_manager_->ProvidePictureBuffer(video_config_.coded_size);
    if (!picture_buffer) {
      read_cb.Run(DemuxerStream::kAborted, NULL);
      return;
    }
    texture_id = picture_buffer->texture_id;
  }

  if (!channel_->Send(new MediaPipelineMsg_ReadDecodedData(routing_id_, type,
                                                           texture_id))) {
    read_cb.Run(DemuxerStream::kAborted, NULL);
    return;
  }

  decoded_data_read_callbacks_[type] = read_cb;
}

bool IPCMediaPipelineHostImpl::PlatformEnlargesBuffersOnUnderflow() const {
  return PlatformMediaPipeline::EnlargesBuffersOnUnderflow();
}

base::TimeDelta IPCMediaPipelineHostImpl::GetTargetBufferDurationBehind()
    const {
  return PlatformMediaPipeline::GetTargetBufferDurationBehind();
}

base::TimeDelta IPCMediaPipelineHostImpl::GetTargetBufferDurationAhead() const {
  return PlatformMediaPipeline::GetTargetBufferDurationAhead();
}

void IPCMediaPipelineHostImpl::OnReadRawData(int64_t position, int size) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());

  TRACE_EVENT_ASYNC_BEGIN0("IPC_MEDIA", "ReadRawData", this);

  if (!shared_raw_data_->IsSufficient(size)) {
    DLOG(ERROR) << "Unexpected call to " << __FUNCTION__;
    channel_->Send(new MediaPipelineMsg_RawDataReady(
        routing_id_, media::DataSource::kReadError));
    TRACE_EVENT_ASYNC_END0("IPC_MEDIA", "ReadRawData", this);
  }

  data_source_->Read(position,
                     size,
                     shared_raw_data_->memory(),
                     media::BindToCurrentLoop(base::Bind(
                         &IPCMediaPipelineHostImpl::OnReadRawDataFinished,
                         weak_ptr_factory_.GetWeakPtr())));
}

void IPCMediaPipelineHostImpl::OnReadRawDataFinished(int size) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  DCHECK(shared_raw_data_->IsSufficient(size) ||
         size == media::DataSource::kReadError);

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
  DCHECK(task_runner_->RunsTasksOnCurrentThread());

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
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  return routing_id_ != MSG_ROUTING_NONE;
}

void IPCMediaPipelineHostImpl::OnDecodedDataReady(
    const MediaPipelineMsg_DecodedDataReady_Params& params) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  DCHECK(!is_handling_accelerated_video_decode(params.type) ||
         picture_buffer_manager_);

  if (!is_read_in_progress(params.type)) {
    DLOG(ERROR) << "Unexpected call to " << __FUNCTION__;
    return;
  }

  switch (params.status) {
    case media::MediaDataStatus::kOk: {
      scoped_refptr<media::DecoderBuffer> buffer;
      if (is_handling_accelerated_video_decode(params.type)) {
        scoped_ptr<media::AutoReleasedPassThroughDecoderTexture>
            wrapped_texture = picture_buffer_manager_->CreateWrappedTexture(
                params.client_texture_id);
        if (!wrapped_texture) {
          DLOG(ERROR) << "Received invalid picture buffer id "
                      << params.client_texture_id;
          base::ResetAndReturn(&decoded_data_read_callbacks_[params.type])
              .Run(DemuxerStream::kOk, new media::DecoderBuffer(0));
          return;
        }

        // PassThroughDecoderImpl treats 0-sized buffers as a sign of an error.
        buffer = new media::DecoderBuffer(1);
        buffer->set_wrapped_texture(std::move(wrapped_texture));
      } else {
        if (!shared_decoded_data_[params.type]->IsSufficient(params.size)) {
          DLOG(ERROR) << "Unexpected call to " << __FUNCTION__;
          return;
        }

        buffer = media::DecoderBuffer::CopyFrom(
            shared_decoded_data_[params.type]->memory(), params.size);
      }

      buffer->set_timestamp(params.timestamp);
      buffer->set_duration(params.duration);

      base::ResetAndReturn(&decoded_data_read_callbacks_[params.type])
          .Run(DemuxerStream::kOk, buffer);
      break;
    }

    case media::MediaDataStatus::kEOS:
      if (is_handling_accelerated_video_decode(params.type)) {
        // Necessary if the video is looped.
        picture_buffer_manager_->DismissPictureBufferInUse();
      }
      base::ResetAndReturn(&decoded_data_read_callbacks_[params.type])
          .Run(DemuxerStream::kOk, media::DecoderBuffer::CreateEOSBuffer());
      break;

    case media::MediaDataStatus::kConfigChanged:
      if (is_handling_accelerated_video_decode(params.type)) {
        // Decoded data is not returned on config change.
        picture_buffer_manager_->DismissPictureBufferInUse();
      }
      base::ResetAndReturn(&decoded_data_read_callbacks_[params.type])
          .Run(DemuxerStream::kConfigChanged, nullptr);
      break;

    case media::MediaDataStatus::kError:
      // Note that this is a decoder error rather than demuxer error.  Don't
      // return DemuxerStream::kAborted.  Instead, return an empty buffer so
      // that the decoder can signal a decoder error.
      base::ResetAndReturn(&decoded_data_read_callbacks_[params.type])
          .Run(DemuxerStream::kOk, new media::DecoderBuffer(0));
      break;

    default:
      NOTREACHED();
      break;
  }

  TRACE_EVENT_ASYNC_END0(
      "IPC_MEDIA", kDecodedDataReadTraceEventNames[params.type], this);
}

void IPCMediaPipelineHostImpl::OnAudioConfigChanged(
    const media::PlatformAudioConfig& new_audio_config) {
  DVLOG(1) << __FUNCTION__;

  DCHECK(task_runner_->RunsTasksOnCurrentThread());

  if (!is_read_in_progress(media::PLATFORM_MEDIA_AUDIO)) {
    DLOG(ERROR) << "Unexpected call to " << __FUNCTION__;
    return;
  }

  MediaPipelineMsg_DecodedDataReady_Params params;
  HandleConfigChange<media::PLATFORM_MEDIA_AUDIO>(new_audio_config,
                                                  &audio_config_, &params);
  OnDecodedDataReady(params);
}

void IPCMediaPipelineHostImpl::OnVideoConfigChanged(
    const media::PlatformVideoConfig& new_video_config) {
  DVLOG(1) << __FUNCTION__;

  DCHECK(task_runner_->RunsTasksOnCurrentThread());

  if (!is_read_in_progress(media::PLATFORM_MEDIA_VIDEO)) {
    DLOG(ERROR) << "Unexpected call to " << __FUNCTION__;
    return;
  }

  MediaPipelineMsg_DecodedDataReady_Params params;
  if (new_video_config.decoding_mode != video_config_.decoding_mode) {
    DLOG(ERROR) << "New video config tries to change decoding mode.";
    params.status = media::kError;
  } else {
    HandleConfigChange<media::PLATFORM_MEDIA_VIDEO>(new_video_config,
                                                    &video_config_, &params);
  }

  OnDecodedDataReady(params);
}

media::PlatformAudioConfig IPCMediaPipelineHostImpl::audio_config() const {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  return audio_config_;
}

media::PlatformVideoConfig IPCMediaPipelineHostImpl::video_config() const {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  return video_config_;
}

}  // namespace content
