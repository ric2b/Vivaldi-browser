// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_SENDER_CHANNEL_MESSAGE_UTIL_H_
#define CAST_SENDER_CHANNEL_MESSAGE_UTIL_H_

#include <string>

#include "cast/common/channel/message_util.h"
#include "cast/common/channel/proto/cast_channel.pb.h"
#include "platform/base/error.h"

namespace openscreen::cast {

class AuthContext;

proto::CastMessage CreateAuthChallengeMessage(const AuthContext& auth_context);

// |request_id| must be unique for |sender_id|.
ErrorOr<proto::CastMessage> CreateAppAvailabilityRequest(
    const std::string& sender_id,
    int request_id,
    const std::string& app_id);

}  // namespace openscreen::cast

#endif  // CAST_SENDER_CHANNEL_MESSAGE_UTIL_H_
