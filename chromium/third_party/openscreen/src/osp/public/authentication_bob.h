// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OSP_PUBLIC_AUTHENTICATION_BOB_H_
#define OSP_PUBLIC_AUTHENTICATION_BOB_H_

#include <string>
#include <vector>

#include "osp/public/authentication_base.h"

namespace openscreen::osp {

// AuthenticationBob is the PSK consumer, which requires user to input PSK for
// verification.
class AuthenticationBob final : public AuthenticationBase {
 public:
  AuthenticationBob(uint64_t instance_id,
                    AgentFingerprint fingerprint,
                    MessageDemuxer& demuxer,
                    Delegate& delegate);
  AuthenticationBob(const AuthenticationBob&) = delete;
  AuthenticationBob& operator=(const AuthenticationBob&) = delete;
  AuthenticationBob(AuthenticationBob&&) noexcept = delete;
  AuthenticationBob& operator=(AuthenticationBob&&) noexcept = delete;
  virtual ~AuthenticationBob();

  // AuthenticationBase overrides.
  void StartAuthentication() override;
  ErrorOr<size_t> OnStreamMessage(uint64_t instance_id,
                                  uint64_t connection_id,
                                  msgs::Type message_type,
                                  const uint8_t* buffer,
                                  size_t buffer_size,
                                  Clock::time_point now) override;
};

}  // namespace openscreen::osp

#endif  // OSP_PUBLIC_AUTHENTICATION_BOB_H_
