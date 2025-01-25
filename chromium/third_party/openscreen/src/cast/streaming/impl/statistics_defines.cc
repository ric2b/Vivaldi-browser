// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/impl/statistics_defines.h"

namespace openscreen::cast {

StatisticsEventMediaType ToMediaType(StreamType type) {
  switch (type) {
    case StreamType::kUnknown:
      return StatisticsEventMediaType::kUnknown;
    case StreamType::kAudio:
      return StatisticsEventMediaType::kAudio;
    case StreamType::kVideo:
      return StatisticsEventMediaType::kVideo;
  }

  OSP_NOTREACHED();
}

StatisticsEvent::StatisticsEvent(const StatisticsEvent& other) = default;
StatisticsEvent::StatisticsEvent(StatisticsEvent&& other) noexcept = default;
StatisticsEvent& StatisticsEvent::operator=(const StatisticsEvent& other) =
    default;
StatisticsEvent& StatisticsEvent::operator=(StatisticsEvent&& other) = default;

bool StatisticsEvent::operator==(const StatisticsEvent& other) const {
  return frame_id == other.frame_id && type == other.type &&
         media_type == other.media_type &&
         rtp_timestamp == other.rtp_timestamp && size == other.size &&
         timestamp == other.timestamp &&
         received_timestamp == other.received_timestamp;
}

FrameEvent::FrameEvent(const FrameEvent& other) = default;
FrameEvent::FrameEvent(FrameEvent&& other) noexcept = default;
FrameEvent& FrameEvent::operator=(const FrameEvent& other) = default;
FrameEvent& FrameEvent::operator=(FrameEvent&& other) = default;

bool FrameEvent::operator==(const FrameEvent& other) const {
  return StatisticsEvent::operator==(other) && width == other.width &&
         height == other.height && delay_delta == other.delay_delta &&
         key_frame == other.key_frame && target_bitrate == other.target_bitrate;
}

PacketEvent::PacketEvent(const PacketEvent& other) = default;
PacketEvent::PacketEvent(PacketEvent&& other) noexcept = default;
PacketEvent& PacketEvent::operator=(const PacketEvent& other) = default;
PacketEvent& PacketEvent::operator=(PacketEvent&& other) = default;

bool PacketEvent::operator==(const PacketEvent& other) const {
  return StatisticsEvent::operator==(other) && packet_id == other.packet_id &&
         max_packet_id == other.max_packet_id;
}

}  // namespace openscreen::cast
