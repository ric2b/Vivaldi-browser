// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#ifndef PLATFORM_MEDIA_GPU_PIPELINE_MAC_AVF_MEDIA_PIPELINE_H_
#define PLATFORM_MEDIA_GPU_PIPELINE_MAC_AVF_MEDIA_PIPELINE_H_

#include "platform_media/common/feature_toggles.h"

#include <string>

#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "media/base/data_source.h"
#include "platform_media/gpu/pipeline/platform_media_pipeline.h"

namespace media {

class AVFDataBufferQueue;
class AVFMediaDecoder;

class AVFMediaPipeline : public PlatformMediaPipeline {
 public:
  AVFMediaPipeline();
  ~AVFMediaPipeline() override;

  // PlatformMediaPipeline implementation.
  void Initialize(ipc_data_source::Reader source_reader,
                  ipc_data_source::Info source_info,
                  InitializeCB initialize_cb) override;
  void ReadMediaData(IPCDecodingBuffer buffer) override;
  void WillSeek() override {}
  void Seek(base::TimeDelta time, SeekCB seek_cb) override;

 private:
  class MediaDecoderClient;
  friend class MediaDecoderClient;

  void MediaDecoderInitialized(const InitializeCB& initialize_cb, bool success);

  void DataBufferCapacityAvailable();
  void DataBufferCapacityDepleted();

  void SeekDone(SeekCB seek_cb, bool success);

  std::unique_ptr<MediaDecoderClient> media_decoder_client_;
  std::unique_ptr<AVFMediaDecoder> media_decoder_;

  std::unique_ptr<AVFDataBufferQueue>
      media_queues_[kPlatformMediaDataTypeCount];

  base::ThreadChecker thread_checker_;
  base::WeakPtrFactory<AVFMediaPipeline> weak_ptr_factory_;
};


}  // namespace media

#endif  // PLATFORM_MEDIA_GPU_PIPELINE_MAC_AVF_MEDIA_PIPELINE_H_
