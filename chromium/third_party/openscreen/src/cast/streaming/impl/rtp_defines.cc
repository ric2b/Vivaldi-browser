// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/impl/rtp_defines.h"

#include "util/osp_logging.h"

namespace openscreen::cast {

RtpPayloadType GetPayloadType(AudioCodec codec, bool use_android_rtp_hack) {
  if (use_android_rtp_hack) {
    return RtpPayloadType::kAudioHackForAndroidTV;
  }

  switch (codec) {
    case AudioCodec::kAac:
      return RtpPayloadType::kAudioAac;
    case AudioCodec::kOpus:
      return RtpPayloadType::kAudioOpus;
    case AudioCodec::kNotSpecified:
      return RtpPayloadType::kAudioVarious;
    default:
      OSP_NOTREACHED();
  }
}

RtpPayloadType GetPayloadType(VideoCodec codec, bool use_android_rtp_hack) {
  if (use_android_rtp_hack) {
    return RtpPayloadType::kVideoHackForAndroidTV;
  }
  switch (codec) {
    // VP8 and VP9 share the same payload type.
    case VideoCodec::kVp9:
    case VideoCodec::kVp8:
      return RtpPayloadType::kVideoVp8;

    // H264 and HEVC/H265 share the same payload type.
    case VideoCodec::kHevc:  // fallthrough
    case VideoCodec::kH264:
      return RtpPayloadType::kVideoH264;

    case VideoCodec::kAv1:
      return RtpPayloadType::kVideoAv1;

    case VideoCodec::kNotSpecified:
      return RtpPayloadType::kVideoVarious;

    default:
      OSP_NOTREACHED();
  }
}

StreamType ToStreamType(RtpPayloadType type, bool use_android_rtp_hack) {
  if (use_android_rtp_hack) {
    if (type == RtpPayloadType::kAudioHackForAndroidTV) {
      return StreamType::kAudio;
    }
    if (type == RtpPayloadType::kVideoHackForAndroidTV) {
      return StreamType::kVideo;
    }
  }

  if (RtpPayloadType::kAudioFirst <= type &&
      type <= RtpPayloadType::kAudioLast) {
    return StreamType::kAudio;
  }
  if (RtpPayloadType::kVideoFirst <= type &&
      type <= RtpPayloadType::kVideoLast) {
    return StreamType::kVideo;
  }
  return StreamType::kUnknown;
}

bool IsRtpPayloadType(uint8_t raw_byte) {
  switch (static_cast<RtpPayloadType>(raw_byte)) {
    case RtpPayloadType::kAudioOpus:
    case RtpPayloadType::kAudioAac:
    case RtpPayloadType::kAudioPcm16:
    case RtpPayloadType::kAudioVarious:
    case RtpPayloadType::kVideoVp8:
    case RtpPayloadType::kVideoH264:
    case RtpPayloadType::kVideoVp9:
    case RtpPayloadType::kVideoAv1:
    case RtpPayloadType::kVideoVarious:
    case RtpPayloadType::kAudioHackForAndroidTV:
      // Note: RtpPayloadType::kVideoHackForAndroidTV has the same value as
      // kAudioOpus.
      return true;

    case RtpPayloadType::kNull:
      break;
  }
  return false;
}

bool IsRtcpPacketType(uint8_t raw_byte) {
  switch (static_cast<RtcpPacketType>(raw_byte)) {
    case RtcpPacketType::kSenderReport:
    case RtcpPacketType::kReceiverReport:
    case RtcpPacketType::kSourceDescription:
    case RtcpPacketType::kApplicationDefined:
    case RtcpPacketType::kPayloadSpecific:
    case RtcpPacketType::kExtendedReports:
      return true;

    case RtcpPacketType::kNull:
      break;
  }
  return false;
}

}  // namespace openscreen::cast
