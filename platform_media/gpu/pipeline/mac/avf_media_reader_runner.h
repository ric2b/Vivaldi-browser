// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#ifndef PLATFORM_MEDIA_GPU_PIPELINE_MAC_AVF_MEDIA_READER_RUNNER_H_
#define PLATFORM_MEDIA_GPU_PIPELINE_MAC_AVF_MEDIA_READER_RUNNER_H_

#include "platform_media/common/feature_toggles.h"

#include <string>

#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "platform_media/gpu/pipeline/platform_media_pipeline.h"
#include "platform_media/common/platform_media_pipeline_types.h"

namespace media {

class AVFMediaReader;

// The preferred PlatformMediaPipeline implementation for OS X.  Not available
// on all OS X versions, see |IsAvailable()|.
//
// AVFMediaReaderRunner, which lives on the main thread, performs the actual
// media decoding tasks through the synchronous API of AVFMediaRunner.  Thus,
// the main purpose of AVFMediaReaderRunner is to maintain a dedicated thread
// where blocking AVFMediaReader tasks are run and to dispatch requests and
// responses between the main thread and the AVFMediaReader thread.
class AVFMediaReaderRunner : public PlatformMediaPipeline {
 public:
  explicit AVFMediaReaderRunner(IPCDataSource* data_source);
  ~AVFMediaReaderRunner() override;

  // A run-time check is required to determine usability of
  // AVFMediaReaderRunner.
  static bool IsAvailable();

  // PlatformMediaPipeline implementation.
  void Initialize(const std::string& mime_type,
                  const InitializeCB& initialize_cb) override;
  void ReadAudioData(const ReadDataCB& read_audio_data_cb) override;
  void ReadVideoData(const ReadDataCB& read_video_data_cb) override;
  void WillSeek() override;
  void Seek(base::TimeDelta time, const SeekCB& seek_cb) override;

 private:
  void DataReady(PlatformMediaDataType type,
                 const ReadDataCB& read_data_cb,
                 const scoped_refptr<DataBuffer>& data);

  IPCDataSource* const data_source_;

  dispatch_queue_t reader_queue_;
  std::unique_ptr<AVFMediaReader> reader_;

  bool will_seek_;

  scoped_refptr<DataBuffer>
      last_data_buffer_[PlatformMediaDataType::PLATFORM_MEDIA_DATA_TYPE_COUNT];

  base::ThreadChecker thread_checker_;

  base::WeakPtrFactory<AVFMediaReaderRunner> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(AVFMediaReaderRunner);
};

}  // namespace media

#endif  // PLATFORM_MEDIA_GPU_PIPELINE_MAC_AVF_MEDIA_READER_RUNNER_H_
