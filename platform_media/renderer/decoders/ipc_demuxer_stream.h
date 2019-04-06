// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#ifndef PLATFORM_MEDIA_RENDERER_DECODERS_IPC_DEMUXER_STREAM_H_
#define PLATFORM_MEDIA_RENDERER_DECODERS_IPC_DEMUXER_STREAM_H_

#include "platform_media/common/feature_toggles.h"

#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_resource.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/video_decoder_config.h"
#include "platform_media/renderer/pipeline/ipc_media_pipeline_host.h"

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
  Type type() const override;
  void EnableBitstreamConverter() override;
  bool SupportsConfigChanges() override;
  bool enabled() const;
  void set_enabled(bool enabled, base::TimeDelta timestamp);

  void Stop();

 private:
  void DataReady(Status status, scoped_refptr<DecoderBuffer> buffer);

  Type type_;

  IPCMediaPipelineHost* ipc_media_pipeline_host_;
  ReadCB read_cb_;
  bool is_enabled_;

  base::ThreadChecker thread_checker_;
  base::WeakPtrFactory<IPCDemuxerStream> weak_ptr_factory_;
};

}  // namespace media

#endif  // PLATFORM_MEDIA_RENDERER_DECODERS_IPC_DEMUXER_STREAM_H_
