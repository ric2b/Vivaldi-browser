// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#ifndef PLATFORM_MEDIA_GPU_PIPELINE_PLATFORM_MEDIA_PIPELINE_H_
#define PLATFORM_MEDIA_GPU_PIPELINE_PLATFORM_MEDIA_PIPELINE_H_

#include "platform_media/common/feature_toggles.h"

#include <string>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "media/base/media_export.h"
#include "platform_media/common/platform_media_pipeline_types.h"

namespace media {
class DataBuffer;
struct PlatformAudioConfig;
struct PlatformVideoConfig;
}

namespace media {
class IPCDataSource;

// An interface for the media pipeline using decoder infrastructure available
// on specific platforms.  It represents a full decoding pipeline - it reads
// raw input data from a DataSource and outputs decoded and properly formatted
// audio and/or video samples.
class MEDIA_EXPORT PlatformMediaPipeline {
 public:
  using AudioConfigChangedCB =
      base::Callback<void(const PlatformAudioConfig& audio_config)>;
  using VideoConfigChangedCB =
      base::Callback<void(const PlatformVideoConfig& video_config)>;

  using InitializeCB =
      base::Callback<void(bool success,
                          int bitrate,
                          const PlatformMediaTimeInfo& time_info,
                          const PlatformAudioConfig& audio_config,
                          const PlatformVideoConfig& video_config)>;
  // Passing a NULL |buffer| indicates a read/decoding error.
  using ReadDataCB =
      base::Callback<void(const scoped_refptr<DataBuffer>& buffer)>;
  using SeekCB = base::Callback<void(bool success)>;

  virtual ~PlatformMediaPipeline() {}

  virtual void Initialize(const std::string& mime_type,
                          const InitializeCB& initialize_cb) = 0;

  virtual void ReadAudioData(const ReadDataCB& read_audio_data_cb) = 0;
  virtual void ReadVideoData(const ReadDataCB& read_video_data_cb) = 0;

  virtual void WillSeek() = 0;
  virtual void Seek(base::TimeDelta time, const SeekCB& seek_cb) = 0;
};

}  // namespace media

#endif  // PLATFORM_MEDIA_GPU_PIPELINE_PLATFORM_MEDIA_PIPELINE_H_
