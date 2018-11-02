// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#ifndef PLATFORM_MEDIA_GPU_PIPELINE_IPC_MEDIA_PIPELINE_H_
#define PLATFORM_MEDIA_GPU_PIPELINE_IPC_MEDIA_PIPELINE_H_

#include "platform_media/common/feature_toggles.h"

#include "platform_media/common/platform_media_pipeline_types.h"
#include "platform_media/gpu/pipeline/platform_media_pipeline.h"

#include "base/memory/shared_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#if defined(PLATFORM_MEDIA_HWA)
#include "gpu/ipc/service/gpu_command_buffer_stub.h"
#endif
#include "ipc/ipc_listener.h"

#include <map>
#include <string>
#include <vector>

struct MediaPipelineMsg_DecodedDataReady_Params;

namespace IPC {
class Sender;
}

namespace media {
class DataBuffer;
}

namespace media {

#if defined(PLATFORM_MEDIA_HWA)
class GpuCommandBufferStub;
#endif
class IPCDataSourceImpl;
class PlatformMediaPipeline;

// The IPC-facing participant of the media decoding implementation in the GPU
// process.  It owns a PlatformMediaPipeline and uses it to handle media
// decoding requests.  It owns an IPCDataSource object that provides the
// PlatformMediaPipeline with raw media data by requesting it from a DataSource
// living in the render process.
class IPCMediaPipeline : public IPC::Listener {
 public:
  IPCMediaPipeline(IPC::Sender* channel,
                   int32_t routing_id
#if defined(PLATFORM_MEDIA_HWA)
                   , gpu::GpuCommandBufferStub* command_buffer
#endif
                   );
  ~IPCMediaPipeline() override;

  // IPC::Listener implementation.
  bool OnMessageReceived(const IPC::Message& msg) override;

 private:
  // See the state diagram below.  Decoding is only allowed in the DECODING
  // state.
  //
  //   CONSTRUCTED
  //       | OnInitialize()
  //       v
  //     BUSY ----------------------------------------> STOPPED
  //    |     ^               init failure / OnStop()      ^
  //    v     | OnSeek()                                   | OnStop()
  //   DECODING -------------------------------------------
  enum State { CONSTRUCTED, BUSY, DECODING, STOPPED };

  void OnInitialize(int64_t data_source_size,
                    bool is_data_source_streaming,
                    const std::string& mime_type);
  void Initialized(bool success,
                   int bitrate,
                   const PlatformMediaTimeInfo& time_info,
                   const PlatformAudioConfig& audio_config,
                   const PlatformVideoConfig& video_config);

  void OnBufferForDecodedDataReady(PlatformMediaDataType type,
                                   size_t buffer_size,
                                   base::SharedMemoryHandle handle);
  void DecodedDataReady(PlatformMediaDataType type,
                        const scoped_refptr<DataBuffer>& buffer);
#if defined(PLATFORM_MEDIA_HWA)
  void DecodedTextureReady(uint32_t client_texture_id,
                           const scoped_refptr<DataBuffer>& buffer);
#endif
  void DecodedDataReadyImpl(PlatformMediaDataType type,
                            uint32_t client_texture_id,
                            const scoped_refptr<DataBuffer>& buffer);
  void OnAudioConfigChanged(const PlatformAudioConfig& audio_config);
  void OnVideoConfigChanged(const PlatformVideoConfig& video_config);

  void OnWillSeek();
  void OnSeek(base::TimeDelta time);
  void SeekDone(bool success);

  void OnStop();

  void OnReadDecodedData(PlatformMediaDataType type,
                         uint32_t client_texture_id);

  void SendAudioData(MediaDataStatus status,
                     base::TimeDelta timestamp,
                     base::TimeDelta duration);

  bool has_media_type(PlatformMediaDataType type) const {
    DCHECK_LT(type, PlatformMediaDataType::PLATFORM_MEDIA_DATA_TYPE_COUNT);
    return has_media_type_[type];
  }

#if defined(PLATFORM_MEDIA_HWA)
  bool is_handling_accelerated_video_decode(
      PlatformMediaDataType type) const {
    return type == PlatformMediaDataType::PLATFORM_MEDIA_VIDEO &&
           video_config_.decoding_mode ==
               PlatformMediaDecodingMode::HARDWARE;
  }

  bool ClientToServiceTextureId(uint32_t client_texture_id,
                                uint32_t* service_texture_id);
#endif

  State state_;

  bool has_media_type_[PlatformMediaDataType::PLATFORM_MEDIA_DATA_TYPE_COUNT];

  IPC::Sender* const channel_;
  const int32_t routing_id_;

  std::unique_ptr<IPCDataSourceImpl> data_source_;
  std::unique_ptr<PlatformMediaPipeline> media_pipeline_;

  base::ThreadChecker thread_checker_;

  PlatformVideoConfig video_config_;
#if defined(PLATFORM_MEDIA_HWA)
  gpu::GpuCommandBufferStub* command_buffer_;
  // Maps texture ID used in renderer process to one used in GPU process.
  std::map<uint32_t, uint32_t> known_picture_buffers_;
#endif

  // A buffer for decoded media data, shared with the render process.  Filled in
  // the GPU process, consumed in the renderer process.
  std::unique_ptr<base::SharedMemory>
      shared_decoded_data_[PlatformMediaDataType::PLATFORM_MEDIA_DATA_TYPE_COUNT];

  // Holding place for decoded media data when it didn't fit into shared buffer
  // or such buffer is not ready.
  scoped_refptr<DataBuffer>
      pending_output_buffers_[PlatformMediaDataType::PLATFORM_MEDIA_DATA_TYPE_COUNT];

  base::WeakPtrFactory<IPCMediaPipeline> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(IPCMediaPipeline);
};

}  // namespace media

#endif  // PLATFORM_MEDIA_GPU_PIPELINE_IPC_MEDIA_PIPELINE_H_
