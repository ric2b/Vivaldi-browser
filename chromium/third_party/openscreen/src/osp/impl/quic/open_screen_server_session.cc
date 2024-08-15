// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp/impl/quic/open_screen_server_session.h"

#include <utility>

namespace openscreen::osp {

bool OpenScreenCryptoServerStreamHelper::CanAcceptClientHello(
    const quic::CryptoHandshakeMessage& chlo,
    const quic::QuicSocketAddress& client_address,
    const quic::QuicSocketAddress& peer_address,
    const quic::QuicSocketAddress& self_address,
    std::string* error_details) const {
  return true;
}

OpenScreenServerSession::OpenScreenServerSession(
    std::unique_ptr<quic::QuicConnection> connection,
    std::unique_ptr<quic::QuicCompressedCertsCache> compressed_certs_cache,
    OpenScreenSessionBase::Visitor& visitor,
    const quic::QuicCryptoServerConfig& crypto_server_config,
    const quic::QuicConfig& config,
    const quic::ParsedQuicVersionVector& supported_versions)
    : OpenScreenSessionBase(std::move(connection),
                            visitor,
                            config,
                            supported_versions),
      compressed_certs_cache_(std::move(compressed_certs_cache)),
      crypto_server_config_(crypto_server_config) {
  Initialize();
}

OpenScreenServerSession::~OpenScreenServerSession() = default;

std::unique_ptr<quic::QuicCryptoStream>
OpenScreenServerSession::CreateCryptoStream() {
  return CreateCryptoServerStream(&crypto_server_config_,
                                  compressed_certs_cache_.get(), this,
                                  &stream_helper_);
}

}  // namespace openscreen::osp
