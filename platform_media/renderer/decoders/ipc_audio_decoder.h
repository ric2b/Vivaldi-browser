// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2015 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#ifndef PLATFORM_MEDIA_RENDERER_DECODERS_IPC_AUDIO_DECODER_H_
#define PLATFORM_MEDIA_RENDERER_DECODERS_IPC_AUDIO_DECODER_H_

#include "platform_media/common/feature_toggles.h"

#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_checker.h"
#include "media/base/media_export.h"
#include "platform_media/renderer/pipeline/ipc_media_pipeline_host.h"
#include "platform_media/renderer/decoders/ipc_factory.h"

namespace base {
class SequencedTaskRunner;
}

namespace media {

class AudioBus;
class FFmpegURLProtocol;

// Audio decoder based on the IPCMediaPipeline. It decodes in-memory audio file
// data. It is used for Web Audio API, so its usage has to be synchronous.
// The IPCMediaPipeline flow is asynchronous, so IPCAudioDecoder has to use some
// synchronization tricks in order to appear synchronous.
class MEDIA_EXPORT IPCAudioDecoder {
 public:

  explicit IPCAudioDecoder(FFmpegURLProtocol* protocol);
  ~IPCAudioDecoder();

  bool Initialize();

  int Read(std::vector<std::unique_ptr<AudioBus>>* decoded_audio_packets);

  int channels() const { return channels_; }
  int sample_rate() const { return sample_rate_; }
  int number_of_frames() const { return number_of_frames_; }
  base::TimeDelta duration() const { return duration_; }

 private:
  class InMemoryDataSource;

  void OnInitialized(bool success,
                     int bitrate,
                     const PlatformMediaTimeInfo& time_info,
                     const PlatformAudioConfig& audio_config,
                     const PlatformVideoConfig& video_config);
  void ReadInternal();
  void DataReady(DemuxerStream::Status status,
                 scoped_refptr<DecoderBuffer> buffer);

  void RunCreatorOnMainThread(
      DataSource* data_source,
      std::unique_ptr<IPCMediaPipelineHost>* ipc_media_pipeline_host);

  std::unique_ptr<InMemoryDataSource> data_source_;

  int channels_;
  int sample_rate_;
  int number_of_frames_;
  int bytes_per_frame_;
  SampleFormat sample_format_;
  base::TimeDelta duration_;

  std::vector<std::unique_ptr<AudioBus>>* decoded_audio_packets_;
  int frames_read_;

  std::unique_ptr<IPCMediaPipelineHost> ipc_media_pipeline_host_;
  base::WaitableEvent media_task_done_;

  IPCFactory factory_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(IPCAudioDecoder);
};

}  // namespace media

#endif  // PLATFORM_MEDIA_RENDERER_DECODERS_IPC_AUDIO_DECODER_H_
