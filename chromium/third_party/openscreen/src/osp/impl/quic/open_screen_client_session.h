// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OSP_IMPL_QUIC_OPEN_SCREEN_CLIENT_SESSION_H_
#define OSP_IMPL_QUIC_OPEN_SCREEN_CLIENT_SESSION_H_

#include <memory>

#include "osp/impl/quic/open_screen_session_base.h"
#include "quiche/quic/core/quic_crypto_client_stream.h"

namespace openscreen::osp {

class OpenScreenClientSession
    : public OpenScreenSessionBase,
      public quic::QuicCryptoClientStream::ProofHandler {
 public:
  OpenScreenClientSession(
      std::unique_ptr<quic::QuicConnection> connection,
      quic::QuicCryptoClientConfig& crypto_client_config,
      OpenScreenSessionBase::Visitor& visitor,
      const quic::QuicConfig& config,
      const quic::QuicServerId& server_id,
      const quic::ParsedQuicVersionVector& supported_versions);
  OpenScreenClientSession(const OpenScreenClientSession&) = delete;
  OpenScreenClientSession& operator=(const OpenScreenClientSession&) = delete;
  ~OpenScreenClientSession() override;

  // OpenScreenSessionBase overrides. This will initiate the crypto stream.
  void Initialize() override;

 protected:
  // OpenScreenSessionBase interface implementation.
  std::unique_ptr<quic::QuicCryptoStream> CreateCryptoStream() override;

  // ProofHandler interface implementation.
  void OnProofValid(
      const quic::QuicCryptoClientConfig::CachedState& cached) override;
  void OnProofVerifyDetailsAvailable(
      const quic::ProofVerifyDetails& verify_details) override;

 private:
  quic::QuicCryptoClientConfig& crypto_client_config_;
  quic::QuicServerId server_id_;
};

}  // namespace openscreen::osp

#endif  // OSP_IMPL_QUIC_OPEN_SCREEN_CLIENT_SESSION_H_
