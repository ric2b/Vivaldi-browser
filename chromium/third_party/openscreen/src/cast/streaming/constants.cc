// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/constants.h"

#include <ostream>

#include "util/osp_logging.h"

namespace openscreen::cast {

std::ostream& operator<<(std::ostream& os, VideoCodec codec) {
  const char* str = nullptr;
  switch (codec) {
    case VideoCodec::kH264:
      str = "H264";
      break;
    case VideoCodec::kVp8:
      str = "VP8";
      break;
    case VideoCodec::kHevc:
      str = "HEVC";
      break;
    case VideoCodec::kNotSpecified:
      str = "NotSpecified";
      break;
    case VideoCodec::kVp9:
      str = "VP9";
      break;
    case VideoCodec::kAv1:
      str = "AV1";
      break;
    default:
      OSP_NOTREACHED();
  }
  os << str;
  return os;
}

std::ostream& operator<<(std::ostream& os, CastMode mode) {
  const char* str = nullptr;
  switch (mode) {
    case CastMode::kMirroring:
      str = "mirroring";
      break;
    case CastMode::kRemoting:
      str = "remoting";
      break;
    default:
      OSP_NOTREACHED();
  }
  os << str;
  return os;
}

}  // namespace openscreen::cast
