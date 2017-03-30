// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#ifndef CONTENT_RENDERER_MEDIA_IPC_MEDIA_PIPELINE_HOST_IMPL_H_
#define CONTENT_RENDERER_MEDIA_IPC_MEDIA_PIPELINE_HOST_IMPL_H_

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/shared_memory.h"
#include "base/memory/weak_ptr.h"
#include "ipc/ipc_listener.h"
#include "media/filters/ipc_media_pipeline_host.h"
#include "media/filters/platform_media_pipeline_types.h"

struct MediaPipelineMsg_DecodedDataReady_Params;

namespace base {
class SequencedTaskRunner;
}

namespace media {
class DataSource;
class GpuVideoAcceleratorFactories;
}  // namespace media

namespace content {
class GpuChannelHost;

class IPCMediaPipelineHostImpl : public media::IPCMediaPipelineHost,
                                 public IPC::Listener {
 public:
  IPCMediaPipelineHostImpl(
      gpu::GpuChannelHost* channel,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      media::GpuVideoAcceleratorFactories* factories,
      media::DataSource* data_source);
  ~IPCMediaPipelineHostImpl() override;

  // media::IPCMediaPipelineHost implementation.
  void Initialize(const std::string& mimetype,
                  const InitializeCB& callback) override;
  void StartWaitingForSeek() override;
  void Seek(base::TimeDelta time,
            const media::PipelineStatusCB& status_cb) override;
  void Stop() override;
  void ReadDecodedData(
      media::PlatformMediaDataType type,
      const media::DemuxerStream::ReadCB& read_cb) override;
  bool PlatformEnlargesBuffersOnUnderflow() const override;
  base::TimeDelta GetTargetBufferDurationBehind() const override;
  base::TimeDelta GetTargetBufferDurationAhead() const override;
  media::PlatformAudioConfig audio_config() const override;
  media::PlatformVideoConfig video_config() const override;

  // IPC::Listener implementation.
  bool OnMessageReceived(const IPC::Message& msg) override;

 private:
  class PictureBufferManager;
  class SharedData;

  typedef base::Callback<void(media::MediaDataStatus status,
                              const uint8_t* data,
                              int data_size,
                              base::TimeDelta timestamp,
                              base::TimeDelta duration)> DecodedDataErrorCB;

  bool is_connected() const;

  // Calls the Closure callback when the media pipeline initializes.
  void OnInitialized(bool success,
                     int bitrate,
                     const media::PlatformMediaTimeInfo& time_info,
                     const media::PlatformAudioConfig& audio_config,
                     const media::PlatformVideoConfig& video_config);

  // Calls the PipelineStatusCB callback after the seek is finished.
  void OnSought(bool success);

  // Creates and passes to GPU process buffer for transferring raw data.
  void OnRequestBufferForRawData(size_t requested_size);
  // Creates and passes to GPU process buffer for transferring decoded data.
  void OnRequestBufferForDecodedData(media::PlatformMediaDataType type,
                                     size_t requested_size);

  // Performs an actual read operation on the data source and
  // returns the red data to the media pipeline over the IPC.
  void OnReadRawData(int64_t position, int size);
  void OnReadRawDataFinished(int size);

  void OnDecodedDataReady(
      const MediaPipelineMsg_DecodedDataReady_Params& params);
  void OnAudioConfigChanged(const media::PlatformAudioConfig& new_audio_config);
  void OnVideoConfigChanged(const media::PlatformVideoConfig& new_video_config);

  bool is_handling_accelerated_video_decode(
      media::PlatformMediaDataType type) const {
    return type == media::PLATFORM_MEDIA_VIDEO &&
           video_config_.decoding_mode ==
               media::PlatformMediaDecodingMode::HARDWARE;
  }

  bool is_read_in_progress(media::PlatformMediaDataType type) const {
    return !decoded_data_read_callbacks_[type].is_null();
  }

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  media::DataSource* data_source_;
  scoped_refptr<gpu::GpuChannelHost> channel_;
  int32_t routing_id_;

  InitializeCB init_callback_;
  media::PipelineStatusCB seek_callback_;
  media::DemuxerStream::ReadCB
      decoded_data_read_callbacks_[media::PLATFORM_MEDIA_DATA_TYPE_COUNT];

  // A buffer for raw media data, shared with the GPU process. Filled in the
  // renderer process, consumed in the GPU process.
  scoped_ptr<SharedData> shared_raw_data_;

  // Buffers for decoded media data, shared with the GPU process. Filled in
  // the GPU process, consumed in the renderer process.
  scoped_ptr<SharedData>
      shared_decoded_data_[media::PLATFORM_MEDIA_DATA_TYPE_COUNT];

  media::PlatformAudioConfig audio_config_;
  media::PlatformVideoConfig video_config_;

  media::GpuVideoAcceleratorFactories* factories_;

  scoped_ptr<PictureBufferManager> picture_buffer_manager_;

  base::WeakPtrFactory<IPCMediaPipelineHostImpl> weak_ptr_factory_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_IPC_MEDIA_PIPELINE_HOST_IMPL_H_
