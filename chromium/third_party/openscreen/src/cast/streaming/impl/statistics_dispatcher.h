// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STREAMING_IMPL_STATISTICS_DISPATCHER_H_
#define CAST_STREAMING_IMPL_STATISTICS_DISPATCHER_H_

#include <vector>

#include "cast/streaming/impl/statistics_defines.h"
#include "platform/api/time.h"
#include "platform/base/span.h"

namespace openscreen::cast {

class StatisticsCollector;
class Environment;
struct EncodedFrame;
struct RtcpReceiverFrameLogMessage;

// This class is responsible for dispatching statistics events.
class StatisticsDispatcher {
 public:
  explicit StatisticsDispatcher(Environment& environment);

  StatisticsDispatcher(const StatisticsDispatcher&) = delete;
  StatisticsDispatcher& operator=(const StatisticsDispatcher&) = delete;
  StatisticsDispatcher(StatisticsDispatcher&&) noexcept = delete;
  StatisticsDispatcher& operator=(StatisticsDispatcher&&) = delete;
  ~StatisticsDispatcher();

  // Dispatches enqueue events for a given frame.
  void DispatchEnqueueEvents(StreamType stream_type, const EncodedFrame& frame);

  // Dispatches frame log messages.
  void DispatchFrameLogMessages(
      StreamType stream_type,
      const std::vector<RtcpReceiverFrameLogMessage>& messages);

  // Dispatches an ack event.
  void DispatchAckEvent(StreamType stream_type,
                        RtpTimeTicks rtp_timestamp,
                        FrameId frame_id);

 private:
  Environment& environment_;
};

}  // namespace openscreen::cast

#endif  // CAST_STREAMING_IMPL_STATISTICS_DISPATCHER_H_
