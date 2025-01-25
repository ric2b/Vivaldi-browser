// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OSP_IMPL_QUIC_OPEN_SCREEN_SERVER_SESSION_H_
#define OSP_IMPL_QUIC_OPEN_SCREEN_SERVER_SESSION_H_

#include <memory>
#include <string>
#include <vector>

#include "osp/impl/quic/open_screen_session_base.h"
#include "quiche/quic/core/tls_server_handshaker.h"

namespace openscreen::osp {

// This class is used to propagate client certificate to QuicServer.
class TlsServerHandshakerImpl final : public quic::TlsServerHandshaker {
 public:
  TlsServerHandshakerImpl(quic::QuicSession* session,
                          const quic::QuicCryptoServerConfig* crypto_config);
  TlsServerHandshakerImpl(const TlsServerHandshakerImpl&) = delete;
  TlsServerHandshakerImpl& operator=(const TlsServerHandshakerImpl&) = delete;
  ~TlsServerHandshakerImpl() override;

  // quic::TlsServerHandshaker overrides.
  // This will propagate client certificate to QuicServer and the certificate is
  // used in authentication.
  quic::QuicAsyncStatus VerifyCertChain(
      const std::vector<std::string>& certs,
      std::string* error_details,
      std::unique_ptr<quic::ProofVerifyDetails>* details,
      uint8_t* out_alert,
      std::unique_ptr<quic::ProofVerifierCallback> callback) override;
};

class OpenScreenServerSession : public OpenScreenSessionBase {
 public:
  OpenScreenServerSession(
      std::unique_ptr<quic::QuicConnection> connection,
      OpenScreenSessionBase::Visitor& visitor,
      const quic::QuicCryptoServerConfig& crypto_server_config,
      const quic::QuicConfig& config,
      const quic::ParsedQuicVersionVector& supported_versions);
  OpenScreenServerSession(const OpenScreenServerSession&) = delete;
  OpenScreenServerSession& operator=(const OpenScreenServerSession&) = delete;
  ~OpenScreenServerSession() override;

  // quic::Session overrides.
  // This will request client to send agent certificate.
  quic::QuicSSLConfig GetSSLConfig() const override;

 protected:
  // OpenScreenSessionBase interface implementation.
  std::unique_ptr<quic::QuicCryptoStream> CreateCryptoStream() override;

 private:
  const quic::QuicCryptoServerConfig& crypto_server_config_;
};

}  // namespace openscreen::osp

#endif  // OSP_IMPL_QUIC_OPEN_SCREEN_SERVER_SESSION_H_
