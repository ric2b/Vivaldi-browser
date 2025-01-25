// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_RECEIVER_CHANNEL_MESSAGE_UTIL_H_
#define CAST_RECEIVER_CHANNEL_MESSAGE_UTIL_H_

#include <string>

#include "cast/common/channel/message_util.h"
#include "cast/common/channel/proto/cast_channel.pb.h"
#include "platform/base/error.h"

namespace openscreen::cast {

// Creates a message that responds to a previous app availability request with
// ID `request_id` which declares `app_id` to have availability of either
// available or unavailable respectively.
ErrorOr<proto::CastMessage> CreateAppAvailableResponse(
    int request_id,
    const std::string& sender_id,
    const std::string& app_id);
ErrorOr<proto::CastMessage> CreateAppUnavailableResponse(
    int request_id,
    const std::string& sender_id,
    const std::string& app_id);

}  // namespace openscreen::cast

#endif  // CAST_RECEIVER_CHANNEL_MESSAGE_UTIL_H_
