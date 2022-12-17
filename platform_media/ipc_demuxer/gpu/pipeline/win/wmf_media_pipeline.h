// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#ifndef PLATFORM_MEDIA_IPC_DEMUXER_GPU_PIPELINE_WIN_WMF_MEDIA_PIPELINE_H_
#define PLATFORM_MEDIA_IPC_DEMUXER_GPU_PIPELINE_WIN_WMF_MEDIA_PIPELINE_H_

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "platform_media/ipc_demuxer/platform_media_pipeline_types.h"
#include "platform_media/ipc_demuxer/gpu/pipeline/platform_media_pipeline.h"

namespace media {

class WMFMediaPipeline : public PlatformMediaPipeline {
 public:
  WMFMediaPipeline();
  ~WMFMediaPipeline() override;

  void Initialize(ipc_data_source::Info source_info,
                  InitializeCB initialize_cb) override;
  void ReadMediaData(IPCDecodingBuffer buffer) override;
  void WillSeek() override {}
  void Seek(base::TimeDelta time, SeekCB seek_cb) override;

 private:
  class AudioTimestampCalculator;
  class ThreadedImpl;
  friend class ThreadedImpl;

  std::unique_ptr<ThreadedImpl> threaded_impl_;

  scoped_refptr<base::SequencedTaskRunner> media_pipeline_task_runner_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace media

#endif  // PLATFORM_MEDIA_IPC_DEMUXER_GPU_PIPELINE_WIN_WMF_MEDIA_PIPELINE_H_
