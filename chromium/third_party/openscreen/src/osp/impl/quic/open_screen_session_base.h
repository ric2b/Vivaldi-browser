// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OSP_IMPL_QUIC_OPEN_SCREEN_SESSION_BASE_H_
#define OSP_IMPL_QUIC_OPEN_SCREEN_SESSION_BASE_H_

#include <memory>
#include <string>
#include <vector>

#include "osp/impl/quic/quic_connection.h"
#include "osp/impl/quic/quic_stream.h"
#include "quiche/quic/core/quic_connection.h"
#include "quiche/quic/core/quic_crypto_stream.h"
#include "quiche/quic/core/quic_session.h"
#include "quiche/quic/core/quic_types.h"

namespace openscreen::osp {

// A QUIC session for OpenScreen.
class OpenScreenSessionBase : public quic::QuicSession {
 public:
  class Visitor : public quic::QuicSession::Visitor {
   public:
    virtual void OnCryptoHandshakeComplete() = 0;
    virtual void OnIncomingStream(QuicStream* stream) = 0;
    virtual void OnClientCertificates(
        const std::vector<std::string>& certs) = 0;
    virtual QuicConnection::Delegate& GetConnectionDelegate() = 0;
    virtual uint64_t GetInstanceID() = 0;
  };

  OpenScreenSessionBase(
      std::unique_ptr<quic::QuicConnection> connection,
      Visitor& visitor,
      const quic::QuicConfig& config,
      const quic::ParsedQuicVersionVector& supported_versions);
  OpenScreenSessionBase(const OpenScreenSessionBase&) = delete;
  OpenScreenSessionBase& operator=(const OpenScreenSessionBase&) = delete;
  ~OpenScreenSessionBase() override;

  // QuicSession overrides.
  // This will ensure that the crypto session is created.
  void Initialize() override;
  // This will inform Visitor that Handshake is done.
  void OnTlsHandshakeComplete() override;
  // This will offer the custom ALPN.
  std::vector<std::string> GetAlpnsToOffer() const override;
  // This will select the custom ALPN.
  std::vector<absl::string_view>::const_iterator SelectAlpn(
      const std::vector<absl::string_view>& alpns) const override;

  QuicStream* CreateOutgoingStream(QuicStream::Delegate& delegate);
  Visitor& visitor() { return visitor_; }

 protected:
  virtual std::unique_ptr<quic::QuicCryptoStream> CreateCryptoStream() = 0;

  // QuicSession interface implementation.
  quic::QuicCryptoStream* GetMutableCryptoStream() override;
  const quic::QuicCryptoStream* GetCryptoStream() const override;
  quic::QuicStream* CreateIncomingStream(quic::QuicStreamId id) override;
  quic::QuicStream* CreateIncomingStream(quic::PendingStream* pending) override;
  bool ShouldKeepConnectionAlive() const override;

  std::unique_ptr<quic::QuicConnection> connection_;

 private:
  // Used for the crypto handshake.
  std::unique_ptr<quic::QuicCryptoStream> crypto_stream_;
  Visitor& visitor_;
};

}  // namespace openscreen::osp

#endif  // OSP_IMPL_QUIC_OPEN_SCREEN_SESSION_BASE_H_
