// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OSP_IMPL_QUIC_OPEN_SCREEN_SERVER_SESSION_H_
#define OSP_IMPL_QUIC_OPEN_SCREEN_SERVER_SESSION_H_

#include <memory>
#include <string>

#include "osp/impl/quic/open_screen_session_base.h"
#include "quiche/quic/core/quic_crypto_server_stream_base.h"

namespace openscreen::osp {

// A helper class used by the QuicCryptoServerStream.
class OpenScreenCryptoServerStreamHelper
    : public quic::QuicCryptoServerStreamBase::Helper {
 public:
  bool CanAcceptClientHello(const quic::CryptoHandshakeMessage& chlo,
                            const quic::QuicSocketAddress& client_address,
                            const quic::QuicSocketAddress& peer_address,
                            const quic::QuicSocketAddress& self_address,
                            std::string* error_details) const override;
};

class OpenScreenServerSession : public OpenScreenSessionBase {
 public:
  OpenScreenServerSession(
      std::unique_ptr<quic::QuicConnection> connection,
      std::unique_ptr<quic::QuicCompressedCertsCache> compressed_certs_cache,
      OpenScreenSessionBase::Visitor& visitor,
      const quic::QuicCryptoServerConfig& crypto_server_config,
      const quic::QuicConfig& config,
      const quic::ParsedQuicVersionVector& supported_versions);
  OpenScreenServerSession(const OpenScreenServerSession&) = delete;
  OpenScreenServerSession& operator=(const OpenScreenServerSession&) = delete;
  ~OpenScreenServerSession() override;

 protected:
  // OpenScreenSessionBase interface implementation.
  std::unique_ptr<quic::QuicCryptoStream> CreateCryptoStream() override;

 private:
  // Used by QUIC crypto server stream to track most recently compressed certs.
  std::unique_ptr<quic::QuicCompressedCertsCache> compressed_certs_cache_;
  const quic::QuicCryptoServerConfig& crypto_server_config_;
  // This helper is needed when create QuicCryptoServerStream.
  OpenScreenCryptoServerStreamHelper stream_helper_;
};

}  // namespace openscreen::osp

#endif  // OSP_IMPL_QUIC_OPEN_SCREEN_SERVER_SESSION_H_
