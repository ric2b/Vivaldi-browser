// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2015 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA.

#include "platform_media/common/mac/framework_type_conversions.h"

#include "base/logging.h"
#include "base/stl_util.h"

namespace media {

AudioChannelLayoutTag ChromeChannelLayoutToCoreAudioTag(
    ChannelLayout chrome_layout) {
  VLOG(5) << " PROPMEDIA(COMMON) : " << __FUNCTION__
          << " Channel Layout " << ChannelLayoutToString(chrome_layout);
  // Cover usage in the MSE case by providing conversions for values from
  // media::kADTSChannelLayoutTable.  Cover usage in the non-MSE case by
  // providing conversions for values returned by GuessChannelLayout().
  switch (chrome_layout) {
    case CHANNEL_LAYOUT_MONO:
      return kAudioChannelLayoutTag_Mono;

    case CHANNEL_LAYOUT_STEREO:
      return kAudioChannelLayoutTag_Stereo;

    case CHANNEL_LAYOUT_SURROUND:
      return kAudioChannelLayoutTag_MPEG_3_0_A;

    case CHANNEL_LAYOUT_4_0:
      return kAudioChannelLayoutTag_MPEG_4_0_A;

    case CHANNEL_LAYOUT_QUAD:
      return kAudioChannelLayoutTag_Quadraphonic;

    // AudioChannelLayoutTag doesn't seem to distinguish between
    // "Side Left/Right" and "Rear Left/Right" for 5.0 and 5.1 layouts.
    case CHANNEL_LAYOUT_5_0:
    case CHANNEL_LAYOUT_5_0_BACK:
      return kAudioChannelLayoutTag_MPEG_5_0_A;

    case CHANNEL_LAYOUT_5_1:
    case CHANNEL_LAYOUT_5_1_BACK:
      return kAudioChannelLayoutTag_MPEG_5_1_A;

    case CHANNEL_LAYOUT_6_1:
      return kAudioChannelLayoutTag_MPEG_6_1_A;

    case CHANNEL_LAYOUT_7_1:
      return kAudioChannelLayoutTag_MPEG_7_1_C;

    default:
      NOTREACHED() << "Unhandled kADTSChannelLayoutTable value, unhandled "
                      "GuessChannelLayout()-returned value, or unexpected use "
                      "of function";
      return kAudioChannelLayoutTag_Unknown;
  }
}

std::string Loggable(AudioChannelLayoutTag tag) {
  switch (tag) {
    case kAudioChannelLayoutTag_Mono: return "Mono";
    case kAudioChannelLayoutTag_Stereo: return "Stereo";
    case kAudioChannelLayoutTag_MPEG_3_0_A: return "MPEG_3_0_A";
    case kAudioChannelLayoutTag_MPEG_4_0_A: return "MPEG_4_0_A";
    case kAudioChannelLayoutTag_Quadraphonic: return "Quadraphonic";
    case kAudioChannelLayoutTag_MPEG_5_0_A: return "MPEG_5_0_A";
    case kAudioChannelLayoutTag_MPEG_5_1_A: return "MPEG_5_1_A";
    case kAudioChannelLayoutTag_MPEG_6_1_A: return "MPEG_6_1_A";
    case kAudioChannelLayoutTag_MPEG_7_1_C: return "MPEG_7_1_C";
    default: return "Unknown";
  }
}

std::string FourCCToString(uint32_t fourcc) {
  const char buffer[] = {
      (fourcc >> 24) & 0xff,
      (fourcc >> 16) & 0xff,
      (fourcc >>  8) & 0xff,
      (fourcc >>  0) & 0xff,
  };
  return std::string(buffer, base::size(buffer));
}

}  // namespace media
