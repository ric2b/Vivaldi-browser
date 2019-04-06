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

#include "platform_media/test/test_ipc_data_source.h"
#include "platform_media/renderer/pipeline/ipc_media_pipeline_host.h"

#include "media/base/video_decoder_config.h"

namespace media {
class DataBuffer;
}

namespace media {

class PlatformMediaPipeline;

// A trivial implementation of IPCMediaPipelineHost that just delegates to
// PlatformMediaPipeline directly, no IPC involved.
class TestPipelineHost : public IPCMediaPipelineHost {
 public:
  explicit TestPipelineHost(DataSource* data_source);

  ~TestPipelineHost() override;

  void Initialize(const std::string& mimetype,
                  const InitializeCB& callback) override;

  void StartWaitingForSeek() override;

  void Seek(base::TimeDelta time,
            const PipelineStatusCB& status_cb) override;

  void Stop() override;

  void ReadDecodedData(PlatformMediaDataType type,
                       const DemuxerStream::ReadCB& read_cb) override;

  void AppendBuffer(const scoped_refptr<DecoderBuffer>& buffer,
                    const VideoDecoder::DecodeCB& decode_cb) override;
  bool DecodeVideo(const VideoDecoderConfig& config,
                   const DecodeVideoCB& read_cb) override;
  bool HasEnoughData() override;
  int GetMaxDecodeBuffers() override;

  PlatformAudioConfig audio_config() const override;
  PlatformVideoConfig video_config() const override;

 private:


  static void SeekDone(const PipelineStatusCB& status_cb, bool success);

  void Initialized(bool success,
                   int bitrate,
                   const PlatformMediaTimeInfo& time_info,
                   const PlatformAudioConfig& audio_config,
                   const PlatformVideoConfig& video_config);

  void DataReady(PlatformMediaDataType type,
                 const scoped_refptr<DataBuffer>& buffer);

  void DecodedVideoReady(DemuxerStream::Status status,
                         scoped_refptr<DecoderBuffer> buffer);

  void OnAudioConfigChanged(const PlatformAudioConfig& audio_config);

  void OnVideoConfigChanged(const PlatformVideoConfig& video_config);

  TestIPCDataSource data_source_adapter_;
  std::unique_ptr<PlatformMediaPipeline> platform_pipeline_;

  InitializeCB init_cb_;
  DemuxerStream::ReadCB read_cb_[PlatformMediaDataType::PLATFORM_MEDIA_DATA_TYPE_COUNT];
  IPCMediaPipelineHost::DecodeVideoCB decoded_video_frame_callback_;

  PlatformAudioConfig audio_config_;
  PlatformVideoConfig video_config_;

  VideoDecoderConfig config_;
};

}  // namespace media

#endif  // PLATFORM_MEDIA_TEST_TEST_PIPELINE_HOST_H_
