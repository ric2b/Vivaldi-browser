// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STREAMING_STATISTICS_DEFINES_H_
#define CAST_STREAMING_STATISTICS_DEFINES_H_

#include <stddef.h>
#include <stdint.h>

#include "cast/streaming/constants.h"
#include "cast/streaming/frame_id.h"
#include "cast/streaming/rtp_time.h"
#include "platform/api/time.h"
#include "util/enum_name_table.h"

namespace openscreen::cast {

enum class StatisticsEventType : int {
  kUnknown = 0,

  // Sender side frame events.
  kFrameCaptureBegin = 1,
  kFrameCaptureEnd = 2,
  kFrameEncoded = 3,
  kFrameAckReceived = 4,

  // Receiver side frame events.
  kFrameAckSent = 5,
  kFrameDecoded = 6,
  kFramePlayedOut = 7,

  // Sender side packet events.
  kPacketSentToNetwork = 8,
  kPacketRetransmitted = 9,
  kPacketRtxRejected = 10,

  // Receiver side packet events.
  kPacketReceived = 11,

  kNumOfEvents = kPacketReceived + 1
};

enum class StatisticsEventMediaType : int {
  kUnknown = 0,
  kAudio = 1,
  kVideo = 2
};

StatisticsEventMediaType ToMediaType(StreamType type);

extern const EnumNameTable<StatisticsEventType,
                           static_cast<size_t>(
                               StatisticsEventType::kNumOfEvents)>
    kStatisticEventTypeNames;

struct StatisticsEvent {
  constexpr StatisticsEvent(FrameId frame_id,
                            StatisticsEventType type,
                            StatisticsEventMediaType media_type,
                            RtpTimeTicks rtp_timestamp,
                            uint32_t size,
                            Clock::time_point timestamp,
                            Clock::time_point received_timestamp)
      : frame_id(frame_id),
        type(type),
        media_type(media_type),
        rtp_timestamp(rtp_timestamp),
        size(size),
        timestamp(timestamp),
        received_timestamp(received_timestamp) {}

  constexpr StatisticsEvent() = default;
  StatisticsEvent(const StatisticsEvent& other);
  StatisticsEvent(StatisticsEvent&& other) noexcept;
  StatisticsEvent& operator=(const StatisticsEvent& other);
  StatisticsEvent& operator=(StatisticsEvent&& other);
  ~StatisticsEvent() = default;

  bool operator==(const StatisticsEvent& other) const;

  // The frame this event is associated with.
  FrameId frame_id;

  // The type of this frame event.
  StatisticsEventType type = StatisticsEventType::kUnknown;

  // Whether this was audio or video (or unknown).
  StatisticsEventMediaType media_type = StatisticsEventMediaType::kUnknown;

  // The RTP timestamp of the frame this event is associated with.
  RtpTimeTicks rtp_timestamp;

  // Size of this packet, or the frame it is associated with.
  // Note: we use uint32_t instead of size_t for byte count because this struct
  // is sent over IPC which could span 32 & 64 bit processes.
  uint32_t size = 0;

  // Time of event logged.
  Clock::time_point timestamp;

  // Time that the event was received by the sender. Only set for receiver-side
  // events.
  Clock::time_point received_timestamp;
};

struct FrameEvent : public StatisticsEvent {
  constexpr FrameEvent(FrameId frame_id_in,
                       StatisticsEventType type_in,
                       StatisticsEventMediaType media_type_in,
                       RtpTimeTicks rtp_timestamp_in,
                       uint32_t size_in,
                       Clock::time_point timestamp_in,
                       Clock::time_point received_timestamp_in,
                       int width,
                       int height,
                       Clock::duration delay_delta,
                       bool key_frame,
                       int target_bitrate)
      : StatisticsEvent(frame_id_in,
                        type_in,
                        media_type_in,
                        rtp_timestamp_in,
                        size_in,
                        timestamp_in,
                        received_timestamp_in),
        width(width),
        height(height),
        delay_delta(delay_delta),
        key_frame(key_frame),
        target_bitrate(target_bitrate) {}

  constexpr FrameEvent() = default;
  FrameEvent(const FrameEvent& other);
  FrameEvent(FrameEvent&& other) noexcept;
  FrameEvent& operator=(const FrameEvent& other);
  FrameEvent& operator=(FrameEvent&& other);
  ~FrameEvent() = default;

  bool operator==(const FrameEvent& other) const;

  // Resolution of the frame. Only set for video FRAME_CAPTURE_END events.
  int width = 0;
  int height = 0;

  // Only set for FRAME_PLAYOUT events.
  // If this value is zero the frame is rendered on time.
  // If this value is positive it means the frame is rendered late.
  // If this value is negative it means the frame is rendered early.
  Clock::duration delay_delta{};

  // Whether the frame is a key frame. Only set for video FRAME_ENCODED event.
  bool key_frame = false;

  // The requested target bitrate of the encoder at the time the frame is
  // encoded. Only set for video FRAME_ENCODED event.
  int target_bitrate = 0;
};

struct PacketEvent : public StatisticsEvent {
  constexpr PacketEvent(FrameId frame_id_in,
                        StatisticsEventType type_in,
                        StatisticsEventMediaType media_type_in,
                        RtpTimeTicks rtp_timestamp_in,
                        uint32_t size_in,
                        Clock::time_point timestamp_in,
                        Clock::time_point received_timestamp_in,
                        uint16_t packet_id,
                        uint16_t max_packet_id)
      : StatisticsEvent(frame_id_in,
                        type_in,
                        media_type_in,
                        rtp_timestamp_in,
                        size_in,
                        timestamp_in,
                        received_timestamp_in),
        packet_id(packet_id),
        max_packet_id(max_packet_id) {}

  constexpr PacketEvent() = default;
  PacketEvent(const PacketEvent& other);
  PacketEvent(PacketEvent&& other) noexcept;
  PacketEvent& operator=(const PacketEvent& other);
  PacketEvent& operator=(PacketEvent&& other);
  ~PacketEvent() = default;

  bool operator==(const PacketEvent& other) const;

  // The packet this event is associated with.
  uint16_t packet_id = 0;

  // The highest packet ID seen so far at time of event.
  uint16_t max_packet_id = 0;
};

}  // namespace openscreen::cast

#endif  // CAST_STREAMING_STATISTICS_DEFINES_H_
