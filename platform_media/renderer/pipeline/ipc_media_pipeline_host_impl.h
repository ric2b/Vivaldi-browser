// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#ifndef PLATFORM_MEDIA_RENDERER_PIPELINE_IPC_MEDIA_PIPELINE_HOST_IMPL_H_
#define PLATFORM_MEDIA_RENDERER_PIPELINE_IPC_MEDIA_PIPELINE_HOST_IMPL_H_

#include "platform_media/common/feature_toggles.h"

#include "platform_media/common/platform_media_pipeline_types.h"
#include "platform_media/renderer/pipeline/ipc_media_pipeline_host.h"
#include "platform_media/renderer/pipeline/ipc_pipeline_data.h"

#include "base/memory/ref_counted.h"
#include "base/memory/shared_memory.h"
#include "base/memory/weak_ptr.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "ipc/ipc_listener.h"
#include "media/base/video_decoder_config.h"

#include <string>
#include <vector>

struct MediaPipelineMsg_DecodedDataReady_Params;

namespace base {
class SequencedTaskRunner;
}

namespace media {
class DataSource;
class GpuVideoAcceleratorFactories;
}  // namespace media

namespace media {
class GpuChannelHost;

class IPCMediaPipelineHostImpl : public IPCMediaPipelineHost,
                                 public IPC::Listener {
 public:

  IPCMediaPipelineHostImpl(
      gpu::GpuChannelHost* channel,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      GpuVideoAcceleratorFactories* factories,
      DataSource* data_source);
  ~IPCMediaPipelineHostImpl() override;

  // IPCMediaPipelineHost implementation.
  void Initialize(const std::string& mimetype,
                  const InitializeCB& callback) override;
  void StartWaitingForSeek() override;
  void Seek(base::TimeDelta time,
            const PipelineStatusCB& status_cb) override;
  void Stop() override;
  void ReadDecodedData(
      PlatformMediaDataType type,
      const DemuxerStream::ReadCB& read_cb) override;

  void AppendBuffer(const scoped_refptr<DecoderBuffer>& buffer,
                    const VideoDecoder::DecodeCB& decode_cb) override;
  bool DecodeVideo(const VideoDecoderConfig& config,
                   const DecodeVideoCB& read_cb) override;
  bool HasEnoughData() override;
  int GetMaxDecodeBuffers() override;
  PlatformAudioConfig audio_config() const override;
  PlatformVideoConfig video_config() const override;

  // IPC::Listener implementation.
  bool OnMessageReceived(const IPC::Message& msg) override;

 private:
  typedef base::Callback<void(MediaDataStatus status,
                              const uint8_t* data,
                              int data_size,
                              base::TimeDelta timestamp,
                              base::TimeDelta duration)> DecodedDataErrorCB;

  bool is_connected() const;

  // Calls the Closure callback when the media pipeline initializes.
  void OnInitialized(bool success,
                     int bitrate,
                     const PlatformMediaTimeInfo& time_info,
                     const PlatformAudioConfig& audio_config,
                     const PlatformVideoConfig& video_config);

  // Calls the PipelineStatusCB callback after the seek is finished.
  void OnSought(bool success);

  // Creates and passes to GPU process buffer for transferring raw data.
  void OnRequestBufferForRawData(size_t requested_size);
  // Creates and passes to GPU process buffer for transferring decoded data.
  void OnRequestBufferForDecodedData(PlatformMediaDataType type,
                                     size_t requested_size);

  // Performs an actual read operation on the data source and
  // returns the red data to the media pipeline over the IPC.
  void OnReadRawData(int64_t position, int size);
  void OnReadRawDataFinished(int size);

  void DecodedVideoReady(DemuxerStream::Status status,
                         scoped_refptr<DecoderBuffer> buffer);

  void OnDecodedDataReady(
      const MediaPipelineMsg_DecodedDataReady_Params& params);
  void OnAudioConfigChanged(const PlatformAudioConfig& new_audio_config);
  void OnVideoConfigChanged(const PlatformVideoConfig& new_video_config);

  bool is_read_in_progress(PlatformMediaDataType type) const {
    return !decoded_data_read_callbacks_[type].is_null();
  }

  int32_t command_buffer_route_id() const;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  IPCPipelineData pipeline_data_;
  scoped_refptr<gpu::GpuChannelHost> channel_;
  int32_t routing_id_;

  InitializeCB init_callback_;
  PipelineStatusCB seek_callback_;
  DemuxerStream::ReadCB
      decoded_data_read_callbacks_[PlatformMediaDataType::PLATFORM_MEDIA_DATA_TYPE_COUNT];
  IPCMediaPipelineHost::DecodeVideoCB decoded_video_frame_callback_;

  PlatformAudioConfig audio_config_;
  PlatformVideoConfig video_config_;
  VideoDecoderConfig config_;

  GpuVideoAcceleratorFactories* factories_;
  base::WeakPtrFactory<IPCMediaPipelineHostImpl> weak_ptr_factory_;
};

}  // namespace media

#endif  // PLATFORM_MEDIA_RENDERER_PIPELINE_IPC_MEDIA_PIPELINE_HOST_IMPL_H_
