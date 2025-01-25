// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/impl/rtcp_session.h"

#include "util/osp_logging.h"

namespace openscreen::cast {

RtcpSession::RtcpSession(Ssrc sender_ssrc,
                         Ssrc receiver_ssrc,
                         Clock::time_point start_time)
    : sender_ssrc_(sender_ssrc),
      receiver_ssrc_(receiver_ssrc),
      start_time_(start_time),
      ntp_converter_(start_time) {
  OSP_CHECK_NE(sender_ssrc_, kNullSsrc);
  OSP_CHECK_NE(receiver_ssrc_, kNullSsrc);
  OSP_CHECK_NE(sender_ssrc_, receiver_ssrc_);
}

RtcpSession::~RtcpSession() = default;

}  // namespace openscreen::cast
