// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp/impl/quic/quic_connection_impl.h"

#include "osp/impl/quic/quic_connection_factory_base.h"
#include "osp/impl/quic/quic_utils.h"
#include "quiche/quic/core/quic_packets.h"
#include "util/osp_logging.h"
#include "util/trace_logging.h"

namespace openscreen::osp {

QuicConnectionImpl::QuicConnectionImpl(
    QuicConnectionFactoryBase& parent_factory,
    QuicConnection::Delegate& delegate,
    const quic::QuicClock& clock)
    : QuicConnection(delegate), parent_factory_(parent_factory), clock_(clock) {
  TRACE_SCOPED(TraceCategory::kQuic, "QuicConnectionImpl::QuicConnectionImpl");
}

QuicConnectionImpl::~QuicConnectionImpl() {
  if (owns_session_) {
    delete session_;
  }
}

// Passes a received UDP packet to the QUIC implementation.  If this contains
// any stream data, it will be passed automatically to the relevant
// QuicStreamImpl objects.
void QuicConnectionImpl::OnPacketReceived(const UdpPacket& packet) {
  TRACE_SCOPED(TraceCategory::kQuic, "QuicConnectionImpl::OnPacketReceived");
  quic::QuicReceivedPacket quic_packet(
      reinterpret_cast<const char*>(packet.data()), packet.size(),
      clock_.Now());
  session_->ProcessUdpPacket(ToQuicSocketAddress(packet.destination()),
                             ToQuicSocketAddress(packet.source()), quic_packet);
}

QuicStream* QuicConnectionImpl::MakeOutgoingStream(
    QuicStream::Delegate* delegate) {
  TRACE_SCOPED(TraceCategory::kQuic, "QuicConnectionImpl::MakeOutgoingStream");
  OSP_CHECK(session_);

  return session_->CreateOutgoingStream(delegate);
}

void QuicConnectionImpl::Close() {
  TRACE_SCOPED(TraceCategory::kQuic, "QuicConnectionImpl::Close");
  session_->connection()->CloseConnection(
      quic::QUIC_PEER_GOING_AWAY, "session torn down",
      quic::ConnectionCloseBehavior::SEND_CONNECTION_CLOSE_PACKET);
}

void QuicConnectionImpl::OnConnectionClosed(
    quic::QuicConnectionId server_connection_id,
    quic::QuicErrorCode error_code,
    const std::string& error_details,
    quic::ConnectionCloseSource source) {
  TRACE_SCOPED(TraceCategory::kQuic, "QuicConnectionImpl::OnConnectionClosed");
  parent_factory_.OnConnectionClosed(this);
  delegate_.OnConnectionClosed(session_->connection_id().ToString());
  if (dispatcher_) {
    dispatcher_->OnConnectionClosed(server_connection_id, error_code,
                                    error_details, source);
  }
}

void QuicConnectionImpl::OnWriteBlocked(
    quic::QuicBlockedWriterInterface* blocked_writer) {
  if (dispatcher_) {
    dispatcher_->OnWriteBlocked(blocked_writer);
  }
}

void QuicConnectionImpl::OnRstStreamReceived(
    const quic::QuicRstStreamFrame& frame) {
  if (dispatcher_) {
    dispatcher_->OnRstStreamReceived(frame);
  }
}

void QuicConnectionImpl::OnStopSendingReceived(
    const quic::QuicStopSendingFrame& frame) {
  if (dispatcher_) {
    dispatcher_->OnStopSendingReceived(frame);
  }
}

bool QuicConnectionImpl::TryAddNewConnectionId(
    const quic::QuicConnectionId& server_connection_id,
    const quic::QuicConnectionId& new_connection_id) {
  if (dispatcher_) {
    return dispatcher_->TryAddNewConnectionId(server_connection_id,
                                              new_connection_id);
  }

  return false;
}

void QuicConnectionImpl::OnConnectionIdRetired(
    const quic::QuicConnectionId& server_connection_id) {
  if (dispatcher_) {
    return dispatcher_->OnConnectionIdRetired(server_connection_id);
  }
}

void QuicConnectionImpl::OnServerPreferredAddressAvailable(
    const quic::QuicSocketAddress& server_preferred_address) {
  if (dispatcher_) {
    return dispatcher_->OnServerPreferredAddressAvailable(
        server_preferred_address);
  }
}

void QuicConnectionImpl::OnPathDegrading() {}

void QuicConnectionImpl::OnCryptoHandshakeComplete() {
  TRACE_SCOPED(TraceCategory::kQuic,
               "QuicConnectionImpl::OnCryptoHandshakeComplete");
  delegate_.OnCryptoHandshakeComplete(session_->connection_id().ToString());
}

void QuicConnectionImpl::OnIncomingStream(QuicStream* stream) {
  TRACE_SCOPED(TraceCategory::kQuic, "QuicConnectionImpl::OnIncomingStream");
  delegate_.OnIncomingStream(session_->connection_id().ToString(), stream);
}

}  // namespace openscreen::osp
