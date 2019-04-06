// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
//
// This file is an original work developed by Vivaldi Technologies AS

#ifndef PLATFORM_MEDIA_GPU_PIPELINE_LINUX_GSTREAMER_MEDIA_PIPELINE_H_
#define PLATFORM_MEDIA_GPU_PIPELINE_LINUX_GSTREAMER_MEDIA_PIPELINE_H_

#include "platform_media/common/feature_toggles.h"

#include "platform_media/gpu/pipeline/platform_media_pipeline.h"

namespace media {

class DataSource;

class GStreamerMediaPipeline : public PlatformMediaPipeline {
public:

  GStreamerMediaPipeline(DataSource* data_source,
                         const AudioConfigChangedCB& audio_config_changed_cb,
                         const VideoConfigChangedCB& video_config_changed_cb);

  ~GStreamerMediaPipeline() override;

  void Initialize(const std::string& mime_type,
                  const InitializeCB& initialize_cb) override;

  void ReadAudioData(const ReadDataCB& read_audio_data_cb) override;

  void ReadVideoData(const ReadDataCB& read_video_data_cb) override;

  void WillSeek() override;

  void Seek(base::TimeDelta time, const SeekCB& seek_cb) override;

private:
  DataSource* data_source_;
  AudioConfigChangedCB audio_config_changed_cb_;
  VideoConfigChangedCB video_config_changed_cb_;
};

}

#endif  // PLATFORM_MEDIA_GPU_PIPELINE_LINUX_GSTREAMER_MEDIA_PIPELINE_H_
