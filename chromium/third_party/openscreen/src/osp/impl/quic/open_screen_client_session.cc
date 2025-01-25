// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp/impl/quic/open_screen_client_session.h"

#include <utility>

#include "util/osp_logging.h"

namespace openscreen::osp {

OpenScreenClientSession::OpenScreenClientSession(
    std::unique_ptr<quic::QuicConnection> connection,
    quic::QuicCryptoClientConfig& crypto_client_config,
    OpenScreenSessionBase::Visitor& visitor,
    const quic::QuicConfig& config,
    const quic::QuicServerId& server_id,
    const quic::ParsedQuicVersionVector& supported_versions)
    : OpenScreenSessionBase(std::move(connection),
                            visitor,
                            config,
                            supported_versions),
      crypto_client_config_(crypto_client_config),
      server_id_(server_id) {
  Initialize();
}

OpenScreenClientSession::~OpenScreenClientSession() = default;

void OpenScreenClientSession::Initialize() {
  // Initialize must be called first, as that's what generates the crypto
  // stream.
  OpenScreenSessionBase::Initialize();
  static_cast<quic::QuicCryptoClientStreamBase*>(GetMutableCryptoStream())
      ->CryptoConnect();
}

std::unique_ptr<quic::QuicCryptoStream>
OpenScreenClientSession::CreateCryptoStream() {
  return std::make_unique<quic::QuicCryptoClientStream>(
      server_id_, this, nullptr, &crypto_client_config_, this,
      /*has_application_state=*/true);
}

void OpenScreenClientSession::OnProofValid(
    const quic::QuicCryptoClientConfig::CachedState& cached) {
  OSP_LOG_INFO << "Cached server config: " << cached.server_config();
}

// NOTE: Collect and log ProofVerify details when they are implemented.
void OpenScreenClientSession::OnProofVerifyDetailsAvailable(
    const quic::ProofVerifyDetails& verify_details) {}

}  // namespace openscreen::osp
