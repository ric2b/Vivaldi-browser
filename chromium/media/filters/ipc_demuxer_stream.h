// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#ifndef MEDIA_FILTERS_IPC_DEMUXER_STREAM_H_
#define MEDIA_FILTERS_IPC_DEMUXER_STREAM_H_

#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "media/base/demuxer_stream.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/video_decoder_config.h"
#include "media/filters/ipc_media_pipeline_host.h"

namespace media {

class IPCMediaPipelineHost;
struct PlatformAudioConfig;
struct PlatformVideoConfig;

class IPCDemuxerStream : public DemuxerStream {
 public:
  IPCDemuxerStream(Type type, IPCMediaPipelineHost* ipc_media_pipeline_host);
  ~IPCDemuxerStream() override;

  // DemuxerStream's implementation.
  void Read(const ReadCB& read_cb) override;
  AudioDecoderConfig audio_decoder_config() override;
  VideoDecoderConfig video_decoder_config() override;
  VideoRotation video_rotation() override;
  Type type() const override;
  void EnableBitstreamConverter() override;
  bool SupportsConfigChanges() override;
  bool enabled() const override;
  void set_enabled(bool enabled, base::TimeDelta timestamp) override;
  void SetStreamStatusChangeCB(const StreamStatusChangeCB& cb) override;

  void Stop();

 private:
  void DataReady(Status status, const scoped_refptr<DecoderBuffer>& buffer);

  Type type_;

  IPCMediaPipelineHost* ipc_media_pipeline_host_;
  ReadCB read_cb_;
  bool is_enabled_;
  StreamStatusChangeCB stream_status_change_cb_;

  base::ThreadChecker thread_checker_;
  base::WeakPtrFactory<IPCDemuxerStream> weak_ptr_factory_;
};

}  // namespace media

#endif  // MEDIA_FILTERS_IPC_DEMUXER_STREAM_H_
