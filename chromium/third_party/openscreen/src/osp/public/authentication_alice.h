// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OSP_PUBLIC_AUTHENTICATION_ALICE_H_
#define OSP_PUBLIC_AUTHENTICATION_ALICE_H_

#include <string>
#include <vector>

#include "osp/public/authentication_base.h"

namespace openscreen::osp {

// AuthenticationAlice is the PSK presenter, which creates a PSK and presents it
// to the user.
class AuthenticationAlice final : public AuthenticationBase {
 public:
  AuthenticationAlice(uint64_t instance_id,
                      AgentFingerprint fingerprint,
                      std::string_view auth_token,
                      std::string_view password,
                      MessageDemuxer& demuxer,
                      Delegate& delegate);
  AuthenticationAlice(const AuthenticationAlice&) = delete;
  AuthenticationAlice& operator=(const AuthenticationAlice&) = delete;
  AuthenticationAlice(AuthenticationAlice&&) noexcept = delete;
  AuthenticationAlice& operator=(AuthenticationAlice&&) noexcept = delete;
  virtual ~AuthenticationAlice();

  // AuthenticationBase overrides.
  void StartAuthentication() override;
  ErrorOr<size_t> OnStreamMessage(uint64_t instance_id,
                                  uint64_t connection_id,
                                  msgs::Type message_type,
                                  const uint8_t* buffer,
                                  size_t buffer_size,
                                  Clock::time_point now) override;

 private:
  std::string auth_token_;
  std::string password_;
};

}  // namespace openscreen::osp

#endif  // OSP_PUBLIC_AUTHENTICATION_ALICE_H_
