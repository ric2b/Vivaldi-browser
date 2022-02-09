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
#include "platform_media/gpu/data_source/ipc_data_source_impl.h"
#include "platform_media/gpu/pipeline/ipc_decoding_buffer.h"
#include "platform_media/gpu/pipeline/propmedia_gpu_channel.h"

#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "gpu/ipc/common/gpu_channel.mojom.h"

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

class IPCDataSourceImpl;
class PlatformMediaPipeline;

// The IPC-facing participant of the media decoding implementation in the GPU
// process.  It owns a PlatformMediaPipeline and uses it to handle media
// decoding requests.  It owns an IPCDataSource object that provides the
// PlatformMediaPipeline with raw media data by requesting it from a DataSource
// living in the render process.
class IPCMediaPipeline : public gpu::PropmediaGpuChannel::PipelineBase {
 public:
  IPCMediaPipeline();
  ~IPCMediaPipeline() override;

  static std::unique_ptr<gpu::PropmediaGpuChannel::PipelineBase> Create();

  // PipelineBase implementation.
  void Initialize(IPC::Sender* channel,
                  gpu::mojom::VivaldiMediaPipelineParamsPtr params) override;
  bool OnMessageReceived(const IPC::Message& msg) override;

 private:
  // See the state diagram below.  Decoding is only allowed in the DECODING
  // state.
  //
  //   CONSTRUCTED
  //       | Initialize()
  //       v
  //     BUSY ----------------------------------------> STOPPED
  //    |     ^               init failure / OnStop()      ^
  //    v     | OnSeek()                                   | OnStop()
  //   DECODING -------------------------------------------
  enum State { CONSTRUCTED, BUSY, DECODING, STOPPED };

  void Initialized(bool success,
                   int bitrate,
                   const PlatformMediaTimeInfo& time_info,
                   const PlatformAudioConfig& audio_config,
                   const PlatformVideoConfig& video_config);

  void DecodedDataReady(IPCDecodingBuffer buffer);

  // The method is static so we can call the callback with an error status after
  // the week pointer to the pipeline becomes null.
  static void ReadRawData(base::WeakPtr<IPCMediaPipeline> pipeline,
                          ipc_data_source::Buffer source_buffer);

  void OnWillSeek();
  void OnSeek(base::TimeDelta time);
  void SeekDone(bool success);

  void OnStop();

  void OnReadDecodedData(PlatformStreamType stream_type);

  bool has_media_type(PlatformStreamType stream_type) const {
    return GetElem(has_media_type_, stream_type);
  }

  State state_ = CONSTRUCTED;

  bool has_media_type_[kPlatformStreamTypeCount];

  IPC::Sender* channel_ = nullptr;
  int32_t routing_id_ = -1;

  std::unique_ptr<IPCDataSourceImpl> data_source_;
  std::unique_ptr<PlatformMediaPipeline> media_pipeline_;

  base::ThreadChecker thread_checker_;

  // A buffer for decoded media data, shared with the render process.  Filled in
  // the GPU process, consumed in the renderer process.
  IPCDecodingBuffer ipc_decoding_buffers_[kPlatformStreamTypeCount];

  base::WeakPtrFactory<IPCMediaPipeline> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(IPCMediaPipeline);
};

}  // namespace media

#endif  // PLATFORM_MEDIA_GPU_PIPELINE_IPC_MEDIA_PIPELINE_H_
