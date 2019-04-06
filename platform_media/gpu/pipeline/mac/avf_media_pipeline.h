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
  explicit AVFMediaPipeline(IPCDataSource* data_source_);
  ~AVFMediaPipeline() override;

  // PlatformMediaPipeline implementation.
  void Initialize(const std::string& mime_type,
                  const InitializeCB& initialize_cb) override;
  void ReadAudioData(const ReadDataCB& read_audio_data_cb) override;
  void ReadVideoData(const ReadDataCB& read_video_data_cb) override;
  void WillSeek() override {}
  void Seek(base::TimeDelta time, const SeekCB& seek_cb) override;

 private:
  class MediaDecoderClient;
  friend class MediaDecoderClient;

  void MediaDecoderInitialized(const InitializeCB& initialize_cb, bool success);

  void DataBufferCapacityAvailable();
  void DataBufferCapacityDepleted();

  void AudioBufferReady(const ReadDataCB& read_audio_data_cb,
                        const scoped_refptr<DataBuffer>& buffer);
  void VideoBufferReady(const ReadDataCB& read_video_data_cb,
                        const scoped_refptr<DataBuffer>& buffer);

  void SeekDone(const SeekCB& seek_cb, bool success);

  std::unique_ptr<MediaDecoderClient> media_decoder_client_;
  std::unique_ptr<AVFMediaDecoder> media_decoder_;

  std::unique_ptr<AVFDataBufferQueue> audio_queue_;
  std::unique_ptr<AVFDataBufferQueue> video_queue_;

  DataSource* data_source_;

  base::ThreadChecker thread_checker_;
  base::WeakPtrFactory<AVFMediaPipeline> weak_ptr_factory_;
};


}  // namespace media

#endif  // PLATFORM_MEDIA_GPU_PIPELINE_MAC_AVF_MEDIA_PIPELINE_H_
