// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (C) 2015 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#ifndef CONTENT_COMMON_GPU_MEDIA_TEST_PIPELINE_HOST_H_
#define CONTENT_COMMON_GPU_MEDIA_TEST_PIPELINE_HOST_H_

#include <string>

#include "content/common/gpu/media/ipc_data_source.h"
#include "media/filters/ipc_media_pipeline_host.h"

namespace media {
class DataBuffer;
}

namespace content {

class PlatformMediaPipeline;

// A trivial implementation of IPCMediaPipelineHost that just delegates to
// PlatformMediaPipeline directly, no IPC involved.
class TestPipelineHost : public media::IPCMediaPipelineHost {
 public:
  explicit TestPipelineHost(media::DataSource* data_source);
  ~TestPipelineHost() override;

  void Initialize(const std::string& mimetype,
                  const InitializeCB& callback) override;

  void StartWaitingForSeek() override;

  void Seek(base::TimeDelta time,
            const media::PipelineStatusCB& status_cb) override;

  void Stop() override;

  void ReadDecodedData(media::PlatformMediaDataType type,
                       const media::DemuxerStream::ReadCB& read_cb) override;

  bool PlatformEnlargesBuffersOnUnderflow() const override;

  base::TimeDelta GetTargetBufferDurationBehind() const override;
  base::TimeDelta GetTargetBufferDurationAhead() const override;

  media::PlatformAudioConfig audio_config() const override;
  media::PlatformVideoConfig video_config() const override;

 private:
  class DataSourceAdapter : public IPCDataSource {
   public:
    explicit DataSourceAdapter(media::DataSource* data_source)
        : data_source_(data_source) {}
    ~DataSourceAdapter() override;

    void Suspend() override {}
    void Resume() override {}
    void Read(int64_t position,
              int size,
              uint8_t* data,
              const ReadCB& read_cb) override;
    void Stop() override;
    bool GetSize(int64_t* size_out) override;
    bool IsStreaming() override;
    void SetBitrate(int bitrate) override;

   private:
    media::DataSource* const data_source_;
  };

  static void SeekDone(const media::PipelineStatusCB& status_cb, bool success);

  void Initialized(bool success,
                   int bitrate,
                   const media::PlatformMediaTimeInfo& time_info,
                   const media::PlatformAudioConfig& audio_config,
                   const media::PlatformVideoConfig& video_config);

  void DataReady(media::PlatformMediaDataType type,
                 const scoped_refptr<media::DataBuffer>& buffer);

  void OnAudioConfigChanged(const media::PlatformAudioConfig& audio_config);

  void OnVideoConfigChanged(const media::PlatformVideoConfig& video_config);

  DataSourceAdapter data_source_adapter_;
  std::unique_ptr<content::PlatformMediaPipeline> platform_pipeline_;

  InitializeCB init_cb_;
  media::DemuxerStream::ReadCB read_cb_[media::PLATFORM_MEDIA_DATA_TYPE_COUNT];

  media::PlatformAudioConfig audio_config_;
  media::PlatformVideoConfig video_config_;
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_MEDIA_TEST_PIPELINE_HOST_H_
