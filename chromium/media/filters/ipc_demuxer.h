// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#ifndef MEDIA_FILTERS_IPC_DEMUXER_H_
#define MEDIA_FILTERS_IPC_DEMUXER_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "media/base/demuxer.h"
#include "media/base/media_export.h"

class GURL;

namespace base {
class SingleThreadTaskRunner;
}

namespace media {

class IPCDemuxerStream;
class IPCMediaPipelineHost;
struct PlatformAudioConfig;
struct PlatformMediaTimeInfo;
struct PlatformVideoConfig;

// An implementation of the demuxer interface. On its creation it should create
// the media IPC. It is wrapping all of the demuxer functionality, so that the
// IPC part is transparent. It is also responsible for providing the data
// source for the IPCMediaPipelineHost.
class MEDIA_EXPORT IPCDemuxer : public Demuxer {
 public:
  IPCDemuxer(const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
             DataSource* data_source,
             scoped_ptr<IPCMediaPipelineHost> ipc_media_pipeline_host,
             const std::string& content_type,
             const GURL& url);
  ~IPCDemuxer() override;

  // Checks if the content is supported by the IPCDemuxer.
  static bool IsSupported(const std::string& content_type, const GURL& url);

  // Demuxer implementation.
  //
  // All Demuxer functions are expected to be called on |task_runner_|'s
  // thread.
  std::string GetDisplayName() const override;
  void Initialize(DemuxerHost* host,
                  const PipelineStatusCB& status_cb,
                  bool enable_text_tracks) override;
  void Seek(base::TimeDelta time, const PipelineStatusCB& status_cb) override;
  void Stop() override;
  DemuxerStream* GetStream(DemuxerStream::Type type) override;
  base::TimeDelta GetStartTime() const override;
  base::Time GetTimelineOffset() const override;

  // Used to tell the demuxer that a seek request is about to arrive on the
  // media thread.  This lets the demuxer drop everything it was doing and
  // become ready to handle the seek request quickly.
  //
  // This function can be called on any thread.
  void StartWaitingForSeek();

 private:
  // Called when demuxer initiazlizes.
  void OnInitialized(const PipelineStatusCB& callback,
                     bool success,
                     int bitrate,
                     const PlatformMediaTimeInfo& time_info,
                     const PlatformAudioConfig& audio_config,
                     const PlatformVideoConfig& video_config);

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  DemuxerHost* host_;
  DataSource* data_source_;

  std::string mimetype_;

  base::TimeDelta start_time_;

  bool stopping_;

  scoped_ptr<IPCMediaPipelineHost> ipc_media_pipeline_host_;
  scoped_ptr<IPCDemuxerStream> audio_stream_;
  scoped_ptr<IPCDemuxerStream> video_stream_;

  base::WeakPtrFactory<IPCDemuxer> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(IPCDemuxer);
};

}  // namespace media

#endif  // MEDIA_FILTERS_IPC_DEMUXER_H_
