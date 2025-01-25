// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_SENDER_TESTING_TEST_HELPERS_H_
#define CAST_SENDER_TESTING_TEST_HELPERS_H_

#include <cstdint>
#include <string>

#include "cast/sender/channel/message_util.h"

namespace openscreen::cast {
namespace proto {
class CastMessage;
}

void VerifyAppAvailabilityRequest(const proto::CastMessage& message,
                                  const std::string& expected_app_id,
                                  int* request_id_out,
                                  std::string* sender_id_out);
void VerifyAppAvailabilityRequest(const proto::CastMessage& message,
                                  std::string* app_id_out,
                                  int* request_id_out,
                                  std::string* sender_id_out);

proto::CastMessage CreateAppAvailableResponseChecked(
    int request_id,
    const std::string& sender_id,
    const std::string& app_id);
proto::CastMessage CreateAppUnavailableResponseChecked(
    int request_id,
    const std::string& sender_id,
    const std::string& app_id);

}  // namespace openscreen::cast

#endif  // CAST_SENDER_TESTING_TEST_HELPERS_H_
