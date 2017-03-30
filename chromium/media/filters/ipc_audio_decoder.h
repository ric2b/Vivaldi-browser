// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (C) 2015 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#ifndef MEDIA_FILTERS_IPC_AUDIO_DECODER_H_
#define MEDIA_FILTERS_IPC_AUDIO_DECODER_H_

#include "base/lazy_instance.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_checker.h"
#include "media/base/media_export.h"
#include "media/filters/ipc_media_pipeline_host.h"

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
  // TODO(wdzierzanowski): Only necessary because this class currently
  // interferes with AudioFileReaderTest (DNA-45771).
  class MEDIA_EXPORT ScopedDisableForTesting {
   public:
    ScopedDisableForTesting();
    ~ScopedDisableForTesting();
  };

  explicit IPCAudioDecoder(FFmpegURLProtocol* protocol);
  ~IPCAudioDecoder();

  static bool IsAvailable();
  static void Preinitialize(
      const IPCMediaPipelineHost::Creator& ipc_media_pipeline_host_creator,
      const scoped_refptr<base::SequencedTaskRunner>& main_task_runner,
      const scoped_refptr<base::SequencedTaskRunner>& media_task_runner);

  bool Initialize();

  int Read(AudioBus* audio_bus);

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
                 const scoped_refptr<DecoderBuffer>& buffer);

  scoped_ptr<InMemoryDataSource> data_source_;

  int channels_;
  int sample_rate_;
  int number_of_frames_;
  int bytes_per_frame_;
  media::SampleFormat sample_format_;
  base::TimeDelta duration_;

  AudioBus* audio_bus_;
  int frames_read_;

  scoped_ptr<IPCMediaPipelineHost> ipc_media_pipeline_host_;
  base::WaitableEvent media_task_done_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(IPCAudioDecoder);
};

}  // namespace media

#endif  // MEDIA_FILTERS_IPC_AUDIO_DECODER_H_
