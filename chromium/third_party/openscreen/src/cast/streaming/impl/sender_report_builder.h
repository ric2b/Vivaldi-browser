// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STREAMING_IMPL_SENDER_REPORT_BUILDER_H_
#define CAST_STREAMING_IMPL_SENDER_REPORT_BUILDER_H_

#include <stdint.h>

#include <utility>

#include "cast/streaming/impl/rtcp_common.h"
#include "cast/streaming/impl/rtcp_session.h"
#include "cast/streaming/impl/rtp_defines.h"
#include "platform/api/time.h"
#include "platform/base/span.h"

namespace openscreen::cast {

// Builds RTCP packets containing one Sender Report.
class SenderReportBuilder {
 public:
  explicit SenderReportBuilder(RtcpSession& session);
  ~SenderReportBuilder();

  // Serializes the given `sender_report` as a RTCP packet and writes it to
  // `buffer` (which must be kRequiredBufferSize in size). Returns the subspan
  // of `buffer` that contains the result and a StatusReportId the receiver
  // might use in its own reports to reference this specific report.
  std::pair<ByteBuffer, StatusReportId> BuildPacket(
      const RtcpSenderReport& sender_report,
      ByteBuffer buffer) const;

  // Returns the approximate reference time from a recently-built Sender Report,
  // based on the given `report_id` and maximum possible reference time.
  Clock::time_point GetRecentReportTime(StatusReportId report_id,
                                        Clock::time_point on_or_before) const;

  // The required size (in bytes) of the buffer passed to BuildPacket().
  static constexpr int kRequiredBufferSize =
      kRtcpCommonHeaderSize + kRtcpSenderReportSize + kRtcpReportBlockSize;

 private:
  RtcpSession& session_;
};

}  // namespace openscreen::cast

#endif  // CAST_STREAMING_IMPL_SENDER_REPORT_BUILDER_H_
