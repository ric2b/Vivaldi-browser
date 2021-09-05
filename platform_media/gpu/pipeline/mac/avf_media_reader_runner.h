// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#ifndef PLATFORM_MEDIA_GPU_PIPELINE_MAC_AVF_MEDIA_READER_RUNNER_H_
#define PLATFORM_MEDIA_GPU_PIPELINE_MAC_AVF_MEDIA_READER_RUNNER_H_

#include "platform_media/common/feature_toggles.h"

#include "platform_media/gpu/pipeline/platform_media_pipeline.h"

#include "base/mac/scoped_nsobject.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "gpu/gpu_export.h"

@class DataSourceLoader;

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
  explicit AVFMediaReaderRunner();
  ~AVFMediaReaderRunner() override;

  // A run-time check is required to determine usability of
  // AVFMediaReaderRunner.
  static bool IsAvailable();

  static GPU_EXPORT void WarmUp();

  // PlatformMediaPipeline implementation.
  void Initialize(ipc_data_source::Reader source_reader,
                  ipc_data_source::Info source_info,
                  InitializeCB initialize_cb) override;
  void ReadMediaData(IPCDecodingBuffer buffer) override;
  void WillSeek() override;
  void Seek(base::TimeDelta time, SeekCB seek_cb) override;

 private:
  void DataReady(IPCDecodingBuffer buffer);

  base::scoped_nsobject<DataSourceLoader> data_source_loader_;
  dispatch_queue_t reader_queue_;
  std::unique_ptr<AVFMediaReader> reader_;

  bool will_seek_;

  base::ThreadChecker thread_checker_;

  base::WeakPtrFactory<AVFMediaReaderRunner> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(AVFMediaReaderRunner);
};

}  // namespace media

#endif  // PLATFORM_MEDIA_GPU_PIPELINE_MAC_AVF_MEDIA_READER_RUNNER_H_
