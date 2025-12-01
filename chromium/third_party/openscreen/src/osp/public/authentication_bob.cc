// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp/public/authentication_bob.h"

#include <sstream>
#include <utility>

namespace openscreen::osp {

AuthenticationBob::AuthenticationBob(uint64_t instance_id,
                                     AgentFingerprint fingerprint,
                                     MessageDemuxer& demuxer,
                                     Delegate& delegate)
    : AuthenticationBase(instance_id,
                         std::move(fingerprint),
                         demuxer,
                         delegate) {}

AuthenticationBob::~AuthenticationBob() = default;

void AuthenticationBob::StartAuthentication() {
  if (!auth_data_.sender) {
    delegate_.OnAuthenticationFailed(instance_id_,
                                     Error::Code::kNoActiveConnection);
    return;
  }

  msgs::AuthSpake2Handshake message = {
      .initiation_token =
          msgs::AuthInitiationToken{
              .has_token = true,
              .token = auth_data_.auth_token,
          },
      .psk_status = msgs::AuthSpake2PskStatus::kPskNeedsPresentation,
      .public_value = ComputePublicValue(ComputePrivateKey(fingerprint_))};
  auth_data_.sender->WriteMessage(message, &msgs::EncodeAuthSpake2Handshake);
}

ErrorOr<size_t> AuthenticationBob::OnStreamMessage(uint64_t instance_id,
                                                   uint64_t connection_id,
                                                   msgs::Type message_type,
                                                   const uint8_t* buffer,
                                                   size_t buffer_size,
                                                   Clock::time_point now) {
  OSP_CHECK_EQ(instance_id_, instance_id);
  if (!auth_data_.sender) {
    delegate_.OnAuthenticationFailed(instance_id,
                                     Error::Code::kNoActiveConnection);
    return Error::Code::kNoActiveConnection;
  }

  switch (message_type) {
    case msgs::Type::kAuthSpake2Handshake: {
      msgs::AuthSpake2Handshake handshake;
      ssize_t result =
          msgs::DecodeAuthSpake2Handshake(buffer, buffer_size, handshake);
      if (result < 0) {
        if (result == msgs::kParserEOF) {
          return Error::Code::kCborIncompleteMessage;
        }
        Error error{Error::Code::kCborParsing,
                    "Failed to parse AuthSpake2Handshake message."};
        delegate_.OnAuthenticationFailed(instance_id, error);
        return Error::Code::kCborParsing;
      } else {
        auto& initiation_token = handshake.initiation_token;
        if (!initiation_token.has_token ||
            initiation_token.token != auth_data_.auth_token) {
          Error error{Error::Code::kInvalidAnswer,
                      "Authentication failed: initiation token mismatch."};
          delegate_.OnAuthenticationFailed(instance_id, error);
          return result;
        }

        if (handshake.psk_status == msgs::AuthSpake2PskStatus::kPskShown) {
          // Compute and save the shared key for verification later.
          auth_data_.shared_key =
              ComputeSharedKey(ComputePrivateKey(fingerprint_),
                               handshake.public_value, auth_data_.password);
          msgs::AuthSpake2Handshake message = {
              .initiation_token =
                  msgs::AuthInitiationToken{
                      .has_token = true,
                      .token = initiation_token.token,
                  },
              .psk_status = msgs::AuthSpake2PskStatus::kPskInput,
              .public_value =
                  ComputePublicValue(ComputePrivateKey(fingerprint_))};
          auth_data_.sender->WriteMessage(message,
                                          &msgs::EncodeAuthSpake2Handshake);
        } else if (handshake.psk_status ==
                   msgs::AuthSpake2PskStatus::kPskInput) {
          msgs::AuthSpake2Confirmation message = {
              .confirmation_value = ComputeSharedKey(
                  ComputePrivateKey(fingerprint_), handshake.public_value,
                  auth_data_.password)};
          auth_data_.sender->WriteMessage(message,
                                          &msgs::EncodeAuthSpake2Confirmation);
        } else {
          Error error{Error::Code::kInvalidAnswer,
                      "Authentication failed: receive wrong PSK status."};
          delegate_.OnAuthenticationFailed(instance_id, error);
        }
        return result;
      }
    }

    case msgs::Type::kAuthSpake2Confirmation: {
      msgs::AuthSpake2Confirmation confirmation;
      ssize_t result =
          msgs::DecodeAuthSpake2Confirmation(buffer, buffer_size, confirmation);
      if (result < 0) {
        if (result == msgs::kParserEOF) {
          return Error::Code::kCborIncompleteMessage;
        }
        Error error{Error::Code::kCborParsing,
                    "Failed to parse AuthSpake2Confirmation message."};
        delegate_.OnAuthenticationFailed(instance_id, error);
        return Error::Code::kCborParsing;
      } else {
        if (std::equal(auth_data_.shared_key.begin(),
                       auth_data_.shared_key.end(),
                       confirmation.confirmation_value.begin())) {
          msgs::AuthStatus status = {
              .result = msgs::AuthStatusResult::kAuthenticated};
          auth_data_.sender->WriteMessage(status, &msgs::EncodeAuthStatus);
          delegate_.OnAuthenticationSucceed(instance_id);
        } else {
          msgs::AuthStatus status = {.result =
                                         msgs::AuthStatusResult::kProofInvalid};
          auth_data_.sender->WriteMessage(status, &msgs::EncodeAuthStatus);
          Error error{Error::Code::kInvalidAnswer,
                      "Authentication failed: shared key mismatch."};
          delegate_.OnAuthenticationFailed(instance_id, error);
        }
        return result;
      }
    }

    case msgs::Type::kAuthStatus: {
      msgs::AuthStatus status;
      ssize_t result = msgs::DecodeAuthStatus(buffer, buffer_size, status);
      if (result < 0) {
        if (result == msgs::kParserEOF) {
          return Error::Code::kCborIncompleteMessage;
        }
        Error error{Error::Code::kCborParsing,
                    "Failed to parse AuthStatus message."};
        delegate_.OnAuthenticationFailed(instance_id, error);
        return Error::Code::kCborParsing;
      } else {
        if (status.result == msgs::AuthStatusResult::kAuthenticated) {
          delegate_.OnAuthenticationSucceed(instance_id);
        } else {
          std::stringstream ss;
          ss << "Authentication failed: " << status.result;
          Error error{Error::Code::kInvalidAnswer, ss.str()};
          delegate_.OnAuthenticationFailed(instance_id, error);
        }
        return result;
      }
    }

    default: {
      Error error{Error::Code::kCborParsing,
                  "Receives authentication message with unprocessable type."};
      delegate_.OnAuthenticationFailed(instance_id, error);
      return Error::Code::kCborParsing;
    }
  }
}

}  // namespace openscreen::osp
