// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
//
// This file is an original work developed by Vivaldi Technologies AS

#include "platform_media/gpu/pipeline/linux/gstreamer_media_pipeline.h"

namespace media {

GStreamerMediaPipeline::GStreamerMediaPipeline(DataSource* data_source,
                                               const AudioConfigChangedCB& audio_config_changed_cb,
                                               const VideoConfigChangedCB& video_config_changed_cb) :
  data_source_(data_source),
  audio_config_changed_cb_(audio_config_changed_cb),
  video_config_changed_cb_(video_config_changed_cb)
{

}

GStreamerMediaPipeline::~GStreamerMediaPipeline() {

}

void GStreamerMediaPipeline::Initialize(const std::string& mime_type,
                                        const InitializeCB& initialize_cb) {
  DCHECK(data_source_);

  LOG(INFO) << " PROPMEDIA(GPU) : " << __FUNCTION__
            << " mime type " << mime_type;
}

void GStreamerMediaPipeline::ReadAudioData(const ReadDataCB& read_audio_data_cb)  {

}

void GStreamerMediaPipeline::ReadVideoData(const ReadDataCB& read_video_data_cb)  {

}

void GStreamerMediaPipeline::WillSeek()  {

}

void GStreamerMediaPipeline::Seek(base::TimeDelta time, const SeekCB& seek_cb)  {

}

}
