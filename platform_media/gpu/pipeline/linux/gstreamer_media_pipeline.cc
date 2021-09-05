// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
//
// This file is an original work developed by Vivaldi Technologies AS

#include "platform_media/gpu/pipeline/linux/gstreamer_media_pipeline.h"

namespace media {

GStreamerMediaPipeline::GStreamerMediaPipeline() = default;
GStreamerMediaPipeline::~GStreamerMediaPipeline() = default;

void GStreamerMediaPipeline::Initialize(ipc_data_source::Reader source_reader,
                                        ipc_data_source::Info source_info,
                                        InitializeCB initialize_cb) {
  LOG(INFO) << " PROPMEDIA(GPU) : " << __FUNCTION__ << " mime type "
            << source_info.mime_type;
}

void GStreamerMediaPipeline::ReadAudioData(
    const ReadDataCB& read_audio_data_cb) {}

void GStreamerMediaPipeline::ReadVideoData(
    const ReadDataCB& read_video_data_cb) {}

void GStreamerMediaPipeline::WillSeek() {}

void GStreamerMediaPipeline::Seek(base::TimeDelta time, SeekCB seek_cb) {}

}  // namespace media
