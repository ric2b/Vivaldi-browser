// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#ifndef PLATFORM_MEDIA_GPU_DECODERS_MAC_AVF_MEDIA_READER_H_
#define PLATFORM_MEDIA_GPU_DECODERS_MAC_AVF_MEDIA_READER_H_

#include "platform_media/common/feature_toggles.h"

#import <AVFoundation/AVFoundation.h>

#include <string>

#include "base/mac/scoped_nsobject.h"
#include "base/time/time.h"
#include "gpu/gpu_export.h"
#include "media/base/data_buffer.h"
#include "media/base/data_source.h"
#include "platform_media/common/platform_media_pipeline_types.h"
#include "ui/gfx/geometry/size.h"

@class DataSourceLoader;

namespace media {

class IPCDataSource;

// Wraps AVAssetReader and uses it to perform media decoding tasks.
//
// AVFMediaReader takes raw media data as input and outputs decoded audio and
// video data, handling both the demuxing and decoding internally.  Input data
// is provided via an IPCDataSource.
class GPU_EXPORT AVFMediaReader {
 public:
  // Once AVFMediaReader has been constructed, all functions must run on the
  // |queue| passed to the constructor.
  explicit AVFMediaReader(dispatch_queue_t queue);
  ~AVFMediaReader();

  bool Initialize(media::DataSource* data_source, const std::string& mime_type);

  int bitrate() const;
  base::TimeDelta duration() const;
  base::TimeDelta start_time() const;

  bool has_audio_track() const {
    return GetTrack(PlatformMediaDataType::PLATFORM_MEDIA_AUDIO) != nil;
  }
  bool has_video_track() const {
    return GetTrack(PlatformMediaDataType::PLATFORM_MEDIA_VIDEO) != nil;
  }

  AudioStreamBasicDescription audio_stream_format() const;
  CMFormatDescriptionRef video_stream_format() const;
  CGAffineTransform video_transform() const;

  scoped_refptr<DataBuffer> GetNextAudioSample();
  scoped_refptr<DataBuffer> GetNextVideoSample();

  bool Seek(base::TimeDelta time);

 private:
  // A per-track struct wrapping an AVAssetReader, its output, and some state.
  struct StreamReader {
    StreamReader();
    ~StreamReader();

    PlatformMediaDataType type;
    base::scoped_nsobject<AVAssetReader> asset_reader;
    base::scoped_nsobject<AVAssetReaderTrackOutput> output;
    base::TimeDelta expected_next_timestamp;
    bool end_of_stream;

   private:
    DISALLOW_COPY_AND_ASSIGN(StreamReader);
  };

  bool CalculateBitrate();
  bool ResetStreamReaders(base::TimeDelta start_time);
  bool ResetStreamReader(PlatformMediaDataType type,
                         base::TimeDelta start_time);
  bool InitializeOutput(PlatformMediaDataType type);
  NSDictionary* GetOutputSettings(PlatformMediaDataType type);

  AVAssetTrack* GetTrack(PlatformMediaDataType type) const;

  scoped_refptr<DataBuffer> ProcessNextSample(
      PlatformMediaDataType type);
  scoped_refptr<DataBuffer> ReadNextSample(
      PlatformMediaDataType type);

  base::scoped_nsobject<AVURLAsset> asset_;
  base::scoped_nsobject<DataSourceLoader> data_source_loader_;
  StreamReader stream_readers_[PlatformMediaDataType::PLATFORM_MEDIA_DATA_TYPE_COUNT];

  int bitrate_;
  gfx::Size video_coded_size_;

  dispatch_queue_t queue_;

  DISALLOW_COPY_AND_ASSIGN(AVFMediaReader);
};

}  // namespace media

#endif  // PLATFORM_MEDIA_GPU_DECODERS_MAC_AVF_MEDIA_READER_H_
