// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#ifndef PLATFORM_MEDIA_RENDERER_DECODERS_MAC_CORE_AUDIO_DEMUXER_STREAM_H_
#define PLATFORM_MEDIA_RENDERER_DECODERS_MAC_CORE_AUDIO_DEMUXER_STREAM_H_

#include "platform_media/common/feature_toggles.h"

#include <AudioToolbox/AudioFileStream.h>

#include <vector>

#include "media/base/audio_decoder_config.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_resource.h"
#include "platform_media/common/mac/scoped_audio_queue_ref.h"

namespace media {

class CoreAudioDemuxer;

class CoreAudioDemuxerStream : public DemuxerStream {
 public:
  CoreAudioDemuxerStream(CoreAudioDemuxer* demuxer,
                         AudioStreamBasicDescription input_format,
                         UInt32 bit_rate,
                         Type type);
  ~CoreAudioDemuxerStream() override;

  // DemuxerStream implementation.
  void Read(const ReadCB& read_cb) override;
  AudioDecoderConfig audio_decoder_config() override;
  VideoDecoderConfig video_decoder_config() override;
  Type type() const override;
  void EnableBitstreamConverter() override;
  bool SupportsConfigChanges() override;
  bool enabled() const;
  void set_enabled(bool enabled, base::TimeDelta timestamp);

  void Stop();
  void Abort();
  void ReadCompleted(uint8_t* read_data, int size);

  bool Seek(base::TimeDelta time);

 private:
  static void AudioPropertyListenerProc(void* client_data,
                                        AudioFileStreamID audio_file_stream,
                                        AudioFileStreamPropertyID property_id,
                                        UInt32* io_flags);
  static void AudioPacketsProc(
      void* client_data,
      UInt32 number_bytes,
      UInt32 number_packets,
      const void* input_data,
      AudioStreamPacketDescription* packet_descriptions);
  static void AudioQueueOutputCallback(void* client_data,
                                       AudioQueueRef audio_queue,
                                       AudioQueueBufferRef buffer);

  void InitializeAudioDecoderConfig();
  OSStatus EnqueueBuffer();

  CoreAudioDemuxer* demuxer_;  // Owns us.

  AudioDecoderConfig audio_config_;

  AudioTimeStamp time_stamp_;

  ReadCB read_cb_;
  bool is_enabled_;

  bool reading_audio_data_;
  bool is_enqueue_running_;

  AudioQueueBufferRef output_buffer_;
  AudioStreamBasicDescription input_format_;
  AudioStreamBasicDescription output_format_;

  AudioFileStreamID audio_file_stream_;
  ScopedAudioQueueRef audio_queue_;
  AudioQueueBufferRef audio_queue_buffer_;

  std::unique_ptr<std::vector<AudioStreamPacketDescription>> packet_descs_;

  size_t bytes_filled_;    // how many bytes have been filled
  size_t packets_filled_;  // how many packets have been filled
  UInt32 frames_decoded_;
  UInt32 decoded_data_buffer_size_;
  UInt32 bit_rate_;
  bool pending_seek_;
};

}  // namespace media

#endif  // PLATFORM_MEDIA_RENDERER_DECODERS_MAC_CORE_AUDIO_DEMUXER_STREAM_H_
