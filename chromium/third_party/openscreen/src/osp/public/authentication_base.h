// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OSP_PUBLIC_AUTHENTICATION_BASE_H_
#define OSP_PUBLIC_AUTHENTICATION_BASE_H_

#include <array>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "osp/public/agent_certificate.h"
#include "osp/public/message_demuxer.h"
#include "osp/public/protocol_connection.h"
#include "platform/base/error.h"

namespace openscreen::osp {

// There are two kinds of classes for authentication: AuthenticationAlice and
// AuthenticationBob. This class holds common codes for the two classes.
//
// This class and derived classes are specific to SPAKE2. The links for the
// section of the OSP spec and the SPAKE2 RFC:
// https://w3c.github.io/openscreenprotocol/#authentication-with-spake2
// https://datatracker.ietf.org/doc/html/rfc9382
class AuthenticationBase : public MessageDemuxer::MessageCallback {
 public:
  class Delegate {
   public:
    Delegate() = default;
    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;
    Delegate(Delegate&&) noexcept = delete;
    Delegate& operator=(Delegate&&) noexcept = delete;
    virtual ~Delegate() = default;

    // These interfaces are used to notify the delegate whether the
    // authentication is successful or failed.
    virtual void OnAuthenticationSucceed(uint64_t instance_id) = 0;
    virtual void OnAuthenticationFailed(uint64_t instance_id,
                                        const Error& error) = 0;
  };

  AuthenticationBase(uint64_t instance_id,
                     AgentFingerprint fingerprint,
                     MessageDemuxer& demuxer,
                     Delegate& delegate);
  AuthenticationBase(const AuthenticationBase&) = delete;
  AuthenticationBase& operator=(const AuthenticationBase&) = delete;
  AuthenticationBase(AuthenticationBase&&) noexcept = delete;
  AuthenticationBase& operator=(AuthenticationBase&&) noexcept = delete;
  virtual ~AuthenticationBase();

  virtual void StartAuthentication() = 0;
  void SetSender(std::unique_ptr<ProtocolConnection> sender);
  void SetReceiver(std::unique_ptr<ProtocolConnection> receiver);
  void SetAuthenticationToken(const std::string& auth_token);
  void SetPassword(const std::string& password);

 protected:
  struct AuthenticationData {
    std::unique_ptr<ProtocolConnection> sender;
    std::unique_ptr<ProtocolConnection> receiver;
    std::string auth_token;
    std::string password;
    std::array<uint8_t, 64> shared_key = {};
  };

  // This method is used to calculate private key M/N using Agent Fingerprint as
  // input.
  std::vector<uint8_t> ComputePrivateKey(AgentFingerprint fingerprint);
  // This method is used to calculate public value pA/pB using their
  // respective private keys as input.
  std::vector<uint8_t> ComputePublicValue(
      const std::vector<uint8_t>& self_private_key);
  // This method is used to calculate shared secret using its private keys, the
  // other's public value and password as input.
  std::array<uint8_t, 64> ComputeSharedKey(
      const std::vector<uint8_t>& self_private_key,
      const std::vector<uint8_t>& peer_public_value,
      const std::string& password);

  // Used to identify the instance that is being authentiaced by this class.
  uint64_t instance_id_;
  AgentFingerprint fingerprint_;
  AuthenticationData auth_data_;
  Delegate& delegate_;

 private:
  MessageDemuxer::MessageWatch auth_handshake_watch_;
  MessageDemuxer::MessageWatch auth_confirmation_watch_;
  MessageDemuxer::MessageWatch auth_status_watch_;
};

}  // namespace openscreen::osp

#endif  // OSP_PUBLIC_AUTHENTICATION_BASE_H_
