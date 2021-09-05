// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#ifndef PLATFORM_MEDIA_RENDERER_PIPELINE_IPC_MEDIA_PIPELINE_HOST_H_
#define PLATFORM_MEDIA_RENDERER_PIPELINE_IPC_MEDIA_PIPELINE_HOST_H_

#include "platform_media/common/feature_toggles.h"

#include <string>

#include "base/callback_forward.h"
#include "base/sequenced_task_runner.h"
#include "base/time/time.h"
#include "media/base/demuxer_stream.h"
#include "media/base/pipeline_status.h"
#include "media/base/video_decoder.h"
#include "platform_media/common/platform_media_pipeline_types.h"

namespace media {

class DataSource;
class GpuVideoAcceleratorFactories;

// Represents the renderer side of the IPC connection between the IPCDemuxer
// and the IPCMediaPipeline in the GPU process. It is responsible for
// establishing the IPC connection. It provides methods needed by the demuxer
// and the demuxer stream to work - talk to the decoders over the IPC. As well
// as the methods for responding on the requests recived over IPC for the data
// from the data source.
class IPCMediaPipelineHost {
 public:
  using Creator =
      base::RepeatingCallback<std::unique_ptr<IPCMediaPipelineHost>()>;

  using InitializeCB = base::OnceCallback<void(bool success)>;

  virtual ~IPCMediaPipelineHost() {}

  void init_data_source(DataSource* data_source) {
    DCHECK(data_source);
    DCHECK(!data_source_);
    data_source_ = data_source;
  }

  DataSource* data_source() const { return data_source_; }
  int bitrate() const { return bitrate_; }
  PlatformMediaTimeInfo time_info() const { return time_info_; }
  const PlatformAudioConfig& audio_config() const { return audio_config_; }
  const PlatformVideoConfig& video_config() const { return video_config_; }

  virtual void Initialize(const std::string& mimetype,
                          InitializeCB callback) = 0;

  // Used to inform the platform side of the pipeline that a seek request is
  // about to arrive.  This lets the platform drop everything it was doing and
  // become ready to handle the seek request quickly.
  virtual void StartWaitingForSeek() = 0;

  // Performs the seek over the IPC.
  virtual void Seek(base::TimeDelta time,
                    PipelineStatusCallback status_cb) = 0;

  // Starts an asynchronous read of decoded media data over the IPC.
  virtual void ReadDecodedData(PlatformMediaDataType type,
                               DemuxerStream::ReadCB read_cb) = 0;

 protected:
  DataSource* data_source_ = nullptr;
  int bitrate_ = -1;
  PlatformMediaTimeInfo time_info_;
  PlatformAudioConfig audio_config_;
  PlatformVideoConfig video_config_;
};

}  // namespace media

#endif  // PLATFORM_MEDIA_RENDERER_PIPELINE_IPC_MEDIA_PIPELINE_HOST_H_
