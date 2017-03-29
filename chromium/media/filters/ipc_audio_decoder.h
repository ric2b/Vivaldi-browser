// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (C) 2015 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#ifndef MEDIA_FILTERS_IPC_AUDIO_DECODER_H_
#define MEDIA_FILTERS_IPC_AUDIO_DECODER_H_

#include <string>

#include "base/lazy_instance.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_checker.h"
#include "media/base/media_export.h"
#include "media/filters/ipc_media_pipeline_host.h"

namespace media {

class AudioBus;
class FFmpegURLProtocol;
struct PlatformAudioConfig;
struct PlatformMediaTimeInfo;
struct PlatformVideoConfig;

// Audio decoder based on the IPCMediaPipeline. It decodes in-memory audio file
// data. It is used for Web Audio API, so its usage has to be synchronous.
// The IPCMediaPipeline flow is asynchronous, so IPCAudioDecoder has to use some
// synchronization tricks in order to appear synchronous.
class MEDIA_EXPORT IPCAudioDecoder {
 public:
  explicit IPCAudioDecoder(FFmpegURLProtocol* protocol);
  ~IPCAudioDecoder();

  static void Preinitialize(
      const IPCMediaPipelineHost::Creator& ipc_media_pipeline_host_creator,
      const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner);

  bool Initialize();

  int Read(AudioBus* audio_bus);

  int channels() const { return channels_; }
  int sample_rate() const { return sample_rate_; }
  int number_of_frames() const { return number_of_frames_; }
  base::TimeDelta duration() const { return duration_; }

 private:
  class InMemoryDataSource;

  static bool preinitialized_;
  static base::LazyInstance<IPCMediaPipelineHost::Creator>::Leaky
      ipc_media_pipeline_host_creator_;
  static base::LazyInstance<scoped_refptr<base::SingleThreadTaskRunner>>::Leaky
      media_task_runner_;

  void OnInitialized(bool success,
                     int bitrate,
                     const PlatformMediaTimeInfo& time_info,
                     const PlatformAudioConfig& audio_config,
                     const PlatformVideoConfig& video_config);

  void ReadInternal();
  void DataReady(DemuxerStream::Status status,
                 const scoped_refptr<DecoderBuffer>& buffer);

  void DestroyPipelineOnMediaThread();

  scoped_ptr<InMemoryDataSource> data_source_;

  int channels_;
  int sample_rate_;
  int number_of_frames_;
  int bytes_per_frame_;
  media::SampleFormat sample_format_;
  base::TimeDelta duration_;

  bool initialized_successfully_;

  base::WaitableEvent initialization_event_;
  base::WaitableEvent destruction_event_;
  base::WaitableEvent read_event_;

  AudioBus* audio_bus_;
  int frames_read_;

  scoped_ptr<IPCMediaPipelineHost> ipc_media_pipeline_host_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(IPCAudioDecoder);
};

}  // namespace media

#endif  // MEDIA_FILTERS_IPC_AUDIO_DECODER_H_
