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

#include "base/memory/ref_counted.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/memory/weak_ptr.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "ipc/ipc_listener.h"
//#include "media/base/video_decoder_config.h"

#include <string>

struct MediaPipelineMsg_DecodedDataReady_Params;

namespace base {
class SequencedTaskRunner;
}

namespace media {
class DataSource;
}  // namespace media

namespace media {
class GpuChannelHost;

class IPCMediaPipelineHostImpl : public IPCMediaPipelineHost,
                                 public IPC::Listener {
 public:
  explicit IPCMediaPipelineHostImpl(gpu::GpuChannelHost* channel);
  ~IPCMediaPipelineHostImpl() override;

  // IPCMediaPipelineHost implementation.
  void Initialize(const std::string& mimetype,
                  InitializeCB callback) override;
  void StartWaitingForSeek() override;
  void Seek(base::TimeDelta time,
            PipelineStatusCallback status_cb) override;
  void ReadDecodedData(PlatformStreamType stream_type,
                       DemuxerStream::ReadCB read_cb) override;

  // IPC::Listener implementation.
  bool OnMessageReceived(const IPC::Message& msg) override;

 private:
  typedef base::RepeatingCallback<void(MediaDataStatus status,
                                       const uint8_t* data,
                                       int data_size,
                                       base::TimeDelta timestamp,
                                       base::TimeDelta duration)>
      DecodedDataErrorCB;

  bool is_connected() const;

  // Calls the Closure callback when the media pipeline initializes.
  void OnInitialized(bool success,
                     int bitrate,
                     const PlatformMediaTimeInfo& time_info,
                     const PlatformAudioConfig& audio_config,
                     const PlatformVideoConfig& video_config);

  // Calls the PipelineStatusCB callback after the seek is finished.
  void OnSought(bool success);

  // Performs an actual read operation on the data source and
  // returns the red data to the media pipeline over the IPC.
  void OnReadRawData(int64_t tag,
                     int64_t position,
                     int size);
  void OnReadRawDataFinished(
      int64_t tag,
      base::WritableSharedMemoryMapping mapping,
      int size);

  void DecodedVideoReady(DemuxerStream::Status status,
                         scoped_refptr<DecoderBuffer> buffer);

  void OnDecodedDataReady(
      const MediaPipelineMsg_DecodedDataReady_Params& params,
      base::ReadOnlySharedMemoryRegion region =
          base::ReadOnlySharedMemoryRegion());
  void OnAudioConfigChanged(const PlatformAudioConfig& new_audio_config);
  void OnVideoConfigChanged(const PlatformVideoConfig& new_video_config);

  bool is_read_in_progress(PlatformStreamType stream_type) const {
    return !GetElem(decoded_data_read_callbacks_, stream_type).is_null();
  }

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // A buffer for raw media data, shared with the GPU process. Filled in the
  // renderer process, consumed in the GPU process.
  base::WritableSharedMemoryMapping raw_mapping_;

  // Cached buffers for decoded media data, shared with the GPU process. Filled in
  // the GPU process, consumed in the renderer process.
  base::ReadOnlySharedMemoryMapping
      decoded_mappings_[kPlatformStreamTypeCount];

  scoped_refptr<gpu::GpuChannelHost> channel_;
  int32_t routing_id_;

  InitializeCB init_callback_;
  PipelineStatusCallback seek_callback_;
  DemuxerStream::ReadCB
      decoded_data_read_callbacks_[kPlatformStreamTypeCount];
  bool reading_raw_data_ = false;

  base::WeakPtrFactory<IPCMediaPipelineHostImpl> weak_ptr_factory_;
};

}  // namespace media

#endif  // PLATFORM_MEDIA_RENDERER_PIPELINE_IPC_MEDIA_PIPELINE_HOST_IMPL_H_
