// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2015 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#ifndef PLATFORM_MEDIA_TEST_TEST_PIPELINE_HOST_H_
#define PLATFORM_MEDIA_TEST_TEST_PIPELINE_HOST_H_

#include "platform_media/common/feature_toggles.h"

#include <string>

#include "base/memory/weak_ptr.h"
#include "media/base/data_source.h"
#include "platform_media/gpu/data_source/ipc_data_source_impl.h"
#include "platform_media/gpu/pipeline/ipc_decoding_buffer.h"
#include "platform_media/renderer/pipeline/ipc_media_pipeline_host.h"

#include "media/base/video_decoder_config.h"

namespace media {

class DataBuffer;

class PlatformMediaPipeline;
class PlatformMediaPipelineFactory;

// A trivial implementation of IPCMediaPipelineHost that just delegates to
// PlatformMediaPipeline directly, no IPC involved.
class TestPipelineHost : public IPCMediaPipelineHost {
 public:
  TestPipelineHost();
  ~TestPipelineHost() override;

  void Initialize(const std::string& mimetype,
                  InitializeCB callback) override;

  void StartWaitingForSeek() override;

  void Seek(base::TimeDelta time,
            PipelineStatusCallback status_cb) override;

  void ReadDecodedData(PlatformMediaDataType type,
                       DemuxerStream::ReadCB read_cb) override;

 private:
  static void SeekDone(PipelineStatusCallback status_cb, bool success);

  void Initialized(bool success,
                   int bitrate,
                   const PlatformMediaTimeInfo& time_info,
                   const PlatformAudioConfig& audio_config,
                   const PlatformVideoConfig& video_config);

  void DataReady(DemuxerStream::ReadCB read_cb,
                 IPCDecodingBuffer buffer);

  void ReadRawData(ipc_data_source::Buffer buffer);

  void OnReadRawDataDone(ipc_data_source::Buffer buffer,
                         base::WritableSharedMemoryMapping raw_data_mapping,
                         int size);

  std::unique_ptr<PlatformMediaPipelineFactory> platform_pipeline_factory_;
  std::unique_ptr<PlatformMediaPipeline> platform_pipeline_;
  base::WritableSharedMemoryMapping raw_data_mapping_;

  InitializeCB init_cb_;
  IPCDecodingBuffer ipc_decoding_buffers_[kPlatformMediaDataTypeCount];

  base::WeakPtrFactory<TestPipelineHost> weak_ptr_factory_;
};

}  // namespace media

#endif  // PLATFORM_MEDIA_TEST_TEST_PIPELINE_HOST_H_
