// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#ifndef CONTENT_COMMON_GPU_MEDIA_AVF_MEDIA_READER_RUNNER_H_
#define CONTENT_COMMON_GPU_MEDIA_AVF_MEDIA_READER_RUNNER_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "content/common/gpu/media/platform_media_pipeline.h"
#include "media/filters/platform_media_pipeline_types.h"

namespace content {

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
  void ReadVideoData(const ReadDataCB& read_video_data_cb,
                     uint32_t /* texture_id */) override;
  void WillSeek() override;
  void Seek(base::TimeDelta time, const SeekCB& seek_cb) override;

 private:
  void DataReady(media::PlatformMediaDataType type,
                 const ReadDataCB& read_data_cb,
                 const scoped_refptr<media::DataBuffer>& data);

  IPCDataSource* const data_source_;

  dispatch_queue_t reader_queue_;
  scoped_ptr<AVFMediaReader> reader_;

  bool will_seek_;

  scoped_refptr<media::DataBuffer>
      last_data_buffer_[media::PLATFORM_MEDIA_DATA_TYPE_COUNT];

  base::ThreadChecker thread_checker_;

  base::WeakPtrFactory<AVFMediaReaderRunner> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(AVFMediaReaderRunner);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_MEDIA_AVF_MEDIA_READER_RUNNER_H_
