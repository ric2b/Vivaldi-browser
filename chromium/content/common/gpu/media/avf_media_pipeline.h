// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#ifndef CONTENT_COMMON_GPU_MEDIA_AVF_MEDIA_PIPELINE_H_
#define CONTENT_COMMON_GPU_MEDIA_AVF_MEDIA_PIPELINE_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "content/common/gpu/media/platform_media_pipeline.h"

namespace content {

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
  void ReadVideoData(const ReadDataCB& read_video_data_cb,
                     uint32_t /* texture_id */) override;
  void WillSeek() override {}
  void Seek(base::TimeDelta time, const SeekCB& seek_cb) override;

 private:
  class MediaDecoderClient;
  friend class MediaDecoderClient;

  void MediaDecoderInitialized(const InitializeCB& initialize_cb, bool success);

  void DataBufferCapacityAvailable();
  void DataBufferCapacityDepleted();

  void AudioBufferReady(const ReadDataCB& read_audio_data_cb,
                        const scoped_refptr<media::DataBuffer>& buffer);
  void VideoBufferReady(const ReadDataCB& read_video_data_cb,
                        const scoped_refptr<media::DataBuffer>& buffer);

  void SeekDone(const SeekCB& seek_cb, bool success);

  scoped_ptr<MediaDecoderClient> media_decoder_client_;
  scoped_ptr<AVFMediaDecoder> media_decoder_;

  scoped_ptr<AVFDataBufferQueue> audio_queue_;
  scoped_ptr<AVFDataBufferQueue> video_queue_;

  IPCDataSource* data_source_;

  base::ThreadChecker thread_checker_;
  base::WeakPtrFactory<AVFMediaPipeline> weak_ptr_factory_;
};


}  // namespace content

#endif  // CONTENT_COMMON_GPU_MEDIA_AVF_MEDIA_PIPELINE_H_
