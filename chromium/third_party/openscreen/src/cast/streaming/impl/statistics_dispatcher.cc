// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/impl/statistics_dispatcher.h"

#include <utility>

#include "cast/streaming/encoded_frame.h"
#include "cast/streaming/impl/rtcp_common.h"
#include "cast/streaming/impl/rtp_defines.h"
#include "cast/streaming/impl/session_config.h"
#include "cast/streaming/impl/statistics_collector.h"
#include "cast/streaming/impl/statistics_defines.h"
#include "cast/streaming/public/environment.h"
#include "platform/base/trivial_clock_traits.h"
#include "util/chrono_helpers.h"
#include "util/osp_logging.h"
#include "util/std_util.h"
#include "util/trace_logging.h"

namespace openscreen::cast {

using clock_operators::operator<<;

StatisticsDispatcher::StatisticsDispatcher(Environment& environment)
    : environment_(environment) {}
StatisticsDispatcher::~StatisticsDispatcher() = default;

void StatisticsDispatcher::DispatchEnqueueEvents(StreamType stream_type,
                                                 const EncodedFrame& frame) {
  if (!environment_.statistics_collector()) {
    return;
  }
  const StatisticsEventMediaType media_type = ToMediaType(stream_type);

  // Submit a capture begin event.
  FrameEvent capture_begin_event;
  capture_begin_event.type = StatisticsEventType::kFrameCaptureBegin;
  capture_begin_event.media_type = media_type;
  capture_begin_event.rtp_timestamp = frame.rtp_timestamp;
  capture_begin_event.timestamp =
      (frame.capture_begin_time > Clock::time_point::min())
          ? frame.capture_begin_time
          : environment_.now();
  environment_.statistics_collector()->CollectFrameEvent(
      std::move(capture_begin_event));

  // Submit a capture end event.
  FrameEvent capture_end_event;
  capture_end_event.type = StatisticsEventType::kFrameCaptureEnd;
  capture_end_event.media_type = media_type;
  capture_end_event.rtp_timestamp = frame.rtp_timestamp;
  capture_end_event.timestamp =
      (frame.capture_end_time > Clock::time_point::min())
          ? frame.capture_end_time
          : environment_.now();
  environment_.statistics_collector()->CollectFrameEvent(
      std::move(capture_end_event));

  // Submit an encoded event.
  FrameEvent encode_event;
  encode_event.timestamp = environment_.now();
  encode_event.type = StatisticsEventType::kFrameEncoded;
  encode_event.media_type = media_type;
  encode_event.rtp_timestamp = frame.rtp_timestamp;
  encode_event.frame_id = frame.frame_id;
  encode_event.size = static_cast<uint32_t>(frame.data.size());
  encode_event.key_frame =
      frame.dependency == openscreen::cast::EncodedFrame::Dependency::kKeyFrame;

  environment_.statistics_collector()->CollectFrameEvent(
      std::move(encode_event));
}

void StatisticsDispatcher::DispatchAckEvent(StreamType stream_type,
                                            RtpTimeTicks rtp_timestamp,
                                            FrameId frame_id) {
  if (!environment_.statistics_collector()) {
    return;
  }

  FrameEvent ack_event;
  ack_event.timestamp = environment_.now();
  ack_event.type = StatisticsEventType::kFrameAckReceived;
  ack_event.media_type = ToMediaType(stream_type);
  ack_event.rtp_timestamp = rtp_timestamp;
  ack_event.frame_id = frame_id;

  environment_.statistics_collector()->CollectFrameEvent(std::move(ack_event));
}

void StatisticsDispatcher::DispatchFrameLogMessages(
    StreamType stream_type,
    const std::vector<RtcpReceiverFrameLogMessage>& messages) {
  if (!environment_.statistics_collector()) {
    return;
  }

  const Clock::time_point now = environment_.now();
  const StatisticsEventMediaType media_type = ToMediaType(stream_type);
  for (const RtcpReceiverFrameLogMessage& log_message : messages) {
    for (const RtcpReceiverEventLogMessage& event_message :
         log_message.messages) {
      switch (event_message.type) {
        case StatisticsEventType::kPacketReceived: {
          PacketEvent event;
          event.timestamp = event_message.timestamp;
          event.received_timestamp = now;
          event.type = event_message.type;
          event.media_type = media_type;
          event.rtp_timestamp = log_message.rtp_timestamp;
          event.packet_id = event_message.packet_id;
          environment_.statistics_collector()->CollectPacketEvent(
              std::move(event));
        } break;

        case StatisticsEventType::kFrameAckSent:
        case StatisticsEventType::kFrameDecoded:
        case StatisticsEventType::kFramePlayedOut: {
          FrameEvent event;
          event.timestamp = event_message.timestamp;
          event.received_timestamp = now;
          event.type = event_message.type;
          event.media_type = media_type;
          event.rtp_timestamp = log_message.rtp_timestamp;
          if (event.type == StatisticsEventType::kFramePlayedOut) {
            event.delay_delta = event_message.delay;
          }
          environment_.statistics_collector()->CollectFrameEvent(
              std::move(event));
        } break;

        default:
          OSP_VLOG << "Received log message via RTCP that we did not expect, "
                      "StatisticsEventType="
                   << static_cast<int>(event_message.type);
          break;
      }
    }
  }
}

}  // namespace openscreen::cast
