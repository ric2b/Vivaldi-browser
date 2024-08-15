// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp/impl/quic/open_screen_session_base.h"

#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "osp/impl/quic/quic_connection_impl.h"
#include "osp/impl/quic/quic_constants.h"
#include "osp/impl/quic/quic_stream_impl.h"
#include "quiche/quic/core/quic_constants.h"
#include "util/osp_logging.h"

namespace openscreen::osp {

OpenScreenSessionBase::OpenScreenSessionBase(
    std::unique_ptr<quic::QuicConnection> connection,
    Visitor& visitor,
    const quic::QuicConfig& config,
    const quic::ParsedQuicVersionVector& supported_versions)
    : QuicSession(connection.get(),
                  &visitor,
                  config,
                  supported_versions,
                  /*num_expected_unidirectional_static_streams*/ 0),
      connection_(std::move(connection)),
      visitor_(visitor) {
  const uint32_t max_streams = (std::numeric_limits<uint32_t>::max() /
                                quic::kMaxAvailableStreamsMultiplier) -
                               1;
  this->config()->SetMaxBidirectionalStreamsToSend(max_streams);
  if (VersionHasIetfQuicFrames(transport_version())) {
    this->config()->SetMaxUnidirectionalStreamsToSend(max_streams);
  }
}

OpenScreenSessionBase::~OpenScreenSessionBase() = default;

void OpenScreenSessionBase::Initialize() {
  crypto_stream_ = CreateCryptoStream();
  QuicSession::Initialize();
}

void OpenScreenSessionBase::OnTlsHandshakeComplete() {
  QuicSession::OnTlsHandshakeComplete();
  visitor_.OnCryptoHandshakeComplete();
}

std::vector<std::string> OpenScreenSessionBase::GetAlpnsToOffer() const {
  return std::vector<std::string>({kOpenScreenProtocolALPN});
}

std::vector<absl::string_view>::const_iterator
OpenScreenSessionBase::SelectAlpn(
    const std::vector<absl::string_view>& alpns) const {
  return std::find(alpns.cbegin(), alpns.cend(), kOpenScreenProtocolALPN);
}

QuicStream* OpenScreenSessionBase::CreateOutgoingStream(
    QuicStream::Delegate* delegate) {
  OSP_CHECK(connection()->connected());
  OSP_CHECK(IsEncryptionEstablished());

  auto stream = std::make_unique<QuicStreamImpl>(
      *delegate, GetNextOutgoingBidirectionalStreamId(), this,
      quic::BIDIRECTIONAL);
  QuicStreamImpl* stream_ptr = stream.get();
  ActivateStream(std::move(stream));
  return stream_ptr;
}

const quic::QuicCryptoStream* OpenScreenSessionBase::GetCryptoStream() const {
  return crypto_stream_.get();
}

quic::QuicCryptoStream* OpenScreenSessionBase::GetMutableCryptoStream() {
  return crypto_stream_.get();
}

quic::QuicStream* OpenScreenSessionBase::CreateIncomingStream(
    quic::QuicStreamId id) {
  OSP_CHECK(connection()->connected());

  auto stream = std::make_unique<QuicStreamImpl>(
      visitor_.GetConnectionDelegate().NextStreamDelegate(
          connection_id().ToString(), id),
      id, this, quic::BIDIRECTIONAL);
  auto* stream_ptr = stream.get();
  ActivateStream(std::move(stream));
  visitor_.OnIncomingStream(stream_ptr);
  return stream_ptr;
}

quic::QuicStream* OpenScreenSessionBase::CreateIncomingStream(
    quic::PendingStream* /*pending*/) {
  OSP_NOTREACHED();
}

bool OpenScreenSessionBase::ShouldKeepConnectionAlive() const {
  // OpenScreen connections stay alive until they're explicitly closed.
  return true;
}

}  // namespace openscreen::osp
