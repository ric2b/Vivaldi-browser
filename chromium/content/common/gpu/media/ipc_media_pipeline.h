// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#ifndef CONTENT_COMMON_GPU_MEDIA_IPC_MEDIA_PIPELINE_H_
#define CONTENT_COMMON_GPU_MEDIA_IPC_MEDIA_PIPELINE_H_

#include <map>
#include <string>
#include <vector>

#include "base/memory/shared_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "content/common/gpu/media/platform_media_pipeline.h"
#include "ipc/ipc_listener.h"
#include "media/filters/platform_media_pipeline_types.h"

struct MediaPipelineMsg_DecodedDataReady_Params;

namespace IPC {
class Sender;
}

namespace media {
class DataBuffer;
}

namespace content {

class GpuCommandBufferStub;
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
                   int32_t routing_id,
                   GpuCommandBufferStub* command_buffer);
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
                   const media::PlatformMediaTimeInfo& time_info,
                   const media::PlatformAudioConfig& audio_config,
                   const media::PlatformVideoConfig& video_config);

  void OnBufferForDecodedDataReady(media::PlatformMediaDataType type,
                                   size_t buffer_size,
                                   base::SharedMemoryHandle handle);
  void DecodedDataReady(media::PlatformMediaDataType type,
                        const scoped_refptr<media::DataBuffer>& buffer);
  void DecodedTextureReady(uint32_t client_texture_id,
                           const scoped_refptr<media::DataBuffer>& buffer);
  void DecodedDataReadyImpl(media::PlatformMediaDataType type,
                            uint32_t client_texture_id,
                            const scoped_refptr<media::DataBuffer>& buffer);
  void OnAudioConfigChanged(const media::PlatformAudioConfig& audio_config);
  void OnVideoConfigChanged(const media::PlatformVideoConfig& video_config);

  void OnWillSeek();
  void OnSeek(base::TimeDelta time);
  void SeekDone(bool success);

  void OnStop();

  void OnReadDecodedData(media::PlatformMediaDataType type,
                         uint32_t client_texture_id);

  void SendAudioData(media::MediaDataStatus status,
                     base::TimeDelta timestamp,
                     base::TimeDelta duration);

  bool has_media_type(media::PlatformMediaDataType type) const {
    DCHECK_LT(type, media::PLATFORM_MEDIA_DATA_TYPE_COUNT);
    return has_media_type_[type];
  }

  bool is_handling_accelerated_video_decode(
      media::PlatformMediaDataType type) const {
    return type == media::PLATFORM_MEDIA_VIDEO &&
           video_config_.decoding_mode ==
               media::PlatformMediaDecodingMode::HARDWARE;
  }

  bool ClientToServiceTextureId(uint32_t client_texture_id,
                                uint32_t* service_texture_id);

  State state_;

  bool has_media_type_[media::PLATFORM_MEDIA_DATA_TYPE_COUNT];

  IPC::Sender* const channel_;
  const int32_t routing_id_;

  scoped_ptr<IPCDataSourceImpl> data_source_;
  scoped_ptr<PlatformMediaPipeline> media_pipeline_;

  base::ThreadChecker thread_checker_;

  media::PlatformVideoConfig video_config_;
  GpuCommandBufferStub* command_buffer_;
  // Maps texture ID used in renderer process to one used in GPU process.
  std::map<uint32_t, uint32_t> known_picture_buffers_;

  // A buffer for decoded media data, shared with the render process.  Filled in
  // the GPU process, consumed in the renderer process.
  scoped_ptr<base::SharedMemory>
      shared_decoded_data_[media::PLATFORM_MEDIA_DATA_TYPE_COUNT];

  // Holding place for decoded media data when it didn't fit into shared buffer
  // or such buffer is not ready.
  scoped_refptr<media::DataBuffer>
      pending_output_buffers_[media::PLATFORM_MEDIA_DATA_TYPE_COUNT];

  base::WeakPtrFactory<IPCMediaPipeline> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(IPCMediaPipeline);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_MEDIA_IPC_MEDIA_PIPELINE_H_
