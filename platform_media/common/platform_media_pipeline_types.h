// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#ifndef PLATFORM_MEDIA_COMMON_PLATFORM_MEDIA_PIPELINE_TYPES_H_
#define PLATFORM_MEDIA_COMMON_PLATFORM_MEDIA_PIPELINE_TYPES_H_

#include "platform_media/common/feature_toggles.h"

#include <array>

#include "media/base/sample_format.h"
#include "media/base/video_frame.h"
#include "media/base/video_transformation.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace media {

// Type of a particular stream in a media container.
enum class PlatformStreamType {
  kAudio,
  kVideo,
};

constexpr size_t kPlatformStreamTypeCount =
    static_cast<size_t>(PlatformStreamType::kVideo) + 1;

constexpr const char* GetStreamTypeName(PlatformStreamType stream_type) {
  return (stream_type == PlatformStreamType::kAudio) ? "Audio" : "Video";
}

// Use this to loop over stream types like
// for (PlatformStreamType stream_type : AllStreamTypes()) {...}
constexpr std::array<PlatformStreamType, kPlatformStreamTypeCount>
AllStreamTypes() {
  return {PlatformStreamType::kAudio, PlatformStreamType::kVideo};
}

// Helper to access plain arrays with media kind indexes.
template <typename T>
constexpr T& GetElem(T (&array)[kPlatformStreamTypeCount],
                     PlatformStreamType stream_type) {
  return array[static_cast<size_t>(stream_type)];
}

enum class MediaDataStatus {
  kOk,
  kEOS,
  kMediaError,
  kConfigChanged,
};

constexpr int kMediaDataStatusCount =
    static_cast<int>(MediaDataStatus::kConfigChanged) + 1;

struct PlatformMediaTimeInfo {
  base::TimeDelta duration;
  base::TimeDelta start_time;
};

struct PlatformAudioConfig {
  static constexpr PlatformStreamType kStreamType = PlatformStreamType::kAudio;
  PlatformAudioConfig()
      : format(kUnknownSampleFormat),
        channel_count(-1),
        samples_per_second(-1) {}

  bool is_valid() const {
    return channel_count > 0 && samples_per_second > 0 &&
           format != kUnknownSampleFormat;
  }

  SampleFormat format;
  int channel_count;
  int samples_per_second;
};

struct PlatformVideoConfig {
  static constexpr PlatformStreamType kStreamType = PlatformStreamType::kVideo;
  struct Plane {
    Plane() : stride(-1), offset(-1), size(-1) {}

    bool is_valid() const { return stride > 0 && offset >= 0 && size > 0; }

    int stride;
    int offset;
    int size;
  };

  PlatformVideoConfig() : rotation(VideoRotation::VIDEO_ROTATION_0) {}
  PlatformVideoConfig(const PlatformVideoConfig& other) = default;
  ~PlatformVideoConfig() {}

  bool is_valid() const {
    return !coded_size.IsEmpty() && !visible_rect.IsEmpty() &&
           !natural_size.IsEmpty() && planes[VideoFrame::kYPlane].is_valid() &&
           planes[VideoFrame::kVPlane].is_valid() &&
           planes[VideoFrame::kUPlane].is_valid();
  }

  gfx::Size coded_size;
  gfx::Rect visible_rect;
  gfx::Size natural_size;
  Plane planes[VideoFrame::kMaxPlanes];
  VideoRotation rotation;
};

struct Strides {
  size_t stride_Y;
  size_t stride_UV;
};

}  // namespace media

#endif  // PLATFORM_MEDIA_COMMON_PLATFORM_MEDIA_PIPELINE_TYPES_H_
