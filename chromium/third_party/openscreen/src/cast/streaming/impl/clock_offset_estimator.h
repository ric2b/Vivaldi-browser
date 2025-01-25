// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STREAMING_IMPL_CLOCK_OFFSET_ESTIMATOR_H_
#define CAST_STREAMING_IMPL_CLOCK_OFFSET_ESTIMATOR_H_

#include <memory>
#include <optional>

#include "cast/streaming/public/statistics.h"
#include "cast/streaming/impl/statistics_defines.h"
#include "platform/base/trivial_clock_traits.h"

namespace openscreen::cast {

// Used to estimate the offset between the Sender and Receiver clocks.
class ClockOffsetEstimator {
 public:
  static std::unique_ptr<ClockOffsetEstimator> Create();

  virtual ~ClockOffsetEstimator() {}

  // TODO(issuetracker.google.com/298085631): these should be in a separate
  // header, like Chrome's raw event subscriber pattern.
  // See: //media/cast/logging/raw_event_subscriber.h
  virtual void OnFrameEvent(const FrameEvent& frame_event) = 0;
  virtual void OnPacketEvent(const PacketEvent& packet_event) = 0;

  // Returns nullopt if not enough data is in yet to produce an estimate.
  virtual std::optional<Clock::duration> GetEstimatedOffset() const = 0;
};

}  // namespace openscreen::cast

#endif  // CAST_STREAMING_IMPL_CLOCK_OFFSET_ESTIMATOR_H_
