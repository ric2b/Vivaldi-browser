// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2015 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA.

#ifndef PLATFORM_MEDIA_COMMON_MAC_FRAMEWORK_TYPE_CONVERSIONS_H_
#define PLATFORM_MEDIA_COMMON_MAC_FRAMEWORK_TYPE_CONVERSIONS_H_

#include "platform_media/common/feature_toggles.h"

#import <CoreMedia/CoreMedia.h>

#include "base/time/time.h"
#include "media/base/channel_layout.h"
#include "media/base/media_export.h"

namespace media {

inline const CMTime& CoreMediaGlueCMTimeToCMTime(
    const CMTime& time) {
  return reinterpret_cast<const CMTime&>(time);
}

inline const CMTime& CMTimeToCoreMediaGlueCMTime(
    const CMTime& time) {
  return reinterpret_cast<const CMTime&>(time);
}

inline base::TimeDelta CMTimeToTimeDelta(const CMTime& cm_time) {
  CMTime time = CMTimeToCoreMediaGlueCMTime(cm_time);
  return base::TimeDelta::FromSecondsD(CMTimeGetSeconds(time));
}

inline CMTime TimeDeltaToCoreMediaGlueCMTime(
    const base::TimeDelta& time_delta) {
  // To get the number of 1/600-of-a-second units in |time_delta|, divide the
  // number of microseconds by 1000000 and multiply by 600.
  return CMTimeMake(
      static_cast<double>(time_delta.InMicroseconds()) * (600.0 / 1000000.0),
      600);
}

inline CMTime TimeDeltaToCMTime(const base::TimeDelta& time_delta) {
  return CoreMediaGlueCMTimeToCMTime(
      TimeDeltaToCoreMediaGlueCMTime(time_delta));
}

inline const CMTimeRange& CoreMediaGlueCMTimeRangeToCMTimeRange(
    const CMTimeRange time_range) {
  return reinterpret_cast<const CMTimeRange&>(time_range);
}

MEDIA_EXPORT AudioChannelLayoutTag
ChromeChannelLayoutToCoreAudioTag(ChannelLayout chrome_layout);

MEDIA_EXPORT std::string Loggable(AudioChannelLayoutTag tag);

std::string FourCCToString(uint32_t fourcc);

}  // namespace media

#endif  // PLATFORM_MEDIA_COMMON_MAC_FRAMEWORK_TYPE_CONVERSIONS_H_
