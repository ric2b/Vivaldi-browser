// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STANDALONE_SENDER_CONSTANTS_H_
#define CAST_STANDALONE_SENDER_CONSTANTS_H_

#include "util/chrono_helpers.h"

namespace openscreen::cast {

// How often should the congestion control logic re-evaluate the target encode
// bitrates?
inline constexpr milliseconds kCongestionCheckInterval(500);

// Above what available bandwidth should the high-quality audio bitrate be used?
inline constexpr int kHighBandwidthThreshold = 5 << 20;  // 5 Mbps.

// How often should the file position (media timestamp) be updated on the
// console?
inline constexpr milliseconds kConsoleUpdateInterval(100);

// What is the default maximum bitrate setting?
inline constexpr int kDefaultMaxBitrate = 5 << 20;  // 5 Mbps.

// What is the minimum amount of bandwidth required?
inline constexpr int kMinRequiredBitrate = 384 << 10;  // 384 kbps.

}  // namespace openscreen::cast

#endif  // CAST_STANDALONE_SENDER_CONSTANTS_H_
