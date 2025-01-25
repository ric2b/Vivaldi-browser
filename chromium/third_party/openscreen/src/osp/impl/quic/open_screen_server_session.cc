// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp/impl/quic/open_screen_server_session.h"

#include <utility>

#include "quiche/quic/core/quic_types.h"
#include "quiche/quic/core/quic_versions.h"
#include "util/osp_logging.h"

namespace openscreen::osp {

TlsServerHandshakerImpl::TlsServerHandshakerImpl(
    quic::QuicSession* session,
    const quic::QuicCryptoServerConfig* crypto_config)
    : TlsServerHandshaker(session, crypto_config) {}

TlsServerHandshakerImpl::~TlsServerHandshakerImpl() = default;

quic::QuicAsyncStatus TlsServerHandshakerImpl::VerifyCertChain(
    const std::vector<std::string>& certs,
    std::string* /*error_details*/,
    std::unique_ptr<quic::ProofVerifyDetails>* /*details*/,
    uint8_t* /*out_alert*/,
    std::unique_ptr<quic::ProofVerifierCallback> /*callback*/) {
  static_cast<OpenScreenServerSession*>(session())
      ->visitor()
      .OnClientCertificates(certs);
  return quic::QUIC_SUCCESS;
}

OpenScreenServerSession::OpenScreenServerSession(
    std::unique_ptr<quic::QuicConnection> connection,
    OpenScreenSessionBase::Visitor& visitor,
    const quic::QuicCryptoServerConfig& crypto_server_config,
    const quic::QuicConfig& config,
    const quic::ParsedQuicVersionVector& supported_versions)
    : OpenScreenSessionBase(std::move(connection),
                            visitor,
                            config,
                            supported_versions),
      crypto_server_config_(crypto_server_config) {
  Initialize();
}

OpenScreenServerSession::~OpenScreenServerSession() = default;

quic::QuicSSLConfig OpenScreenServerSession::GetSSLConfig() const {
  quic::QuicSSLConfig ssl_config;
  ssl_config.client_cert_mode = quic::ClientCertMode::kRequest;
  return ssl_config;
}

std::unique_ptr<quic::QuicCryptoStream>
OpenScreenServerSession::CreateCryptoStream() {
  OSP_CHECK_EQ(connection_->version().handshake_protocol,
               quic::HandshakeProtocol::PROTOCOL_TLS1_3);

  return std::make_unique<TlsServerHandshakerImpl>(this,
                                                   &crypto_server_config_);
}

}  // namespace openscreen::osp
