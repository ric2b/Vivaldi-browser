// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OSP_IMPL_QUIC_QUIC_CONNECTION_IMPL_H_
#define OSP_IMPL_QUIC_QUIC_CONNECTION_IMPL_H_

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "osp/impl/quic/open_screen_session_base.h"
#include "osp/impl/quic/quic_connection.h"
#include "osp/impl/quic/quic_dispatcher_impl.h"
#include "platform/api/udp_socket.h"
#include "platform/base/error.h"
#include "quiche/quic/core/quic_clock.h"
#include "quiche/quic/core/quic_connection.h"

namespace openscreen::osp {

class QuicConnectionImpl final : public QuicConnection,
                                 public OpenScreenSessionBase::Visitor {
 public:
  QuicConnectionImpl(std::string_view instance_name,
                     QuicConnection::Delegate& delegate,
                     const quic::QuicClock& clock);
  QuicConnectionImpl(const QuicConnectionImpl&) = delete;
  QuicConnectionImpl& operator=(const QuicConnectionImpl&) = delete;
  ~QuicConnectionImpl() override;

  // QuicConnection overrides.
  void OnPacketReceived(const UdpPacket& packet) override;
  QuicStream* MakeOutgoingStream(QuicStream::Delegate& delegate) override;
  void Close() override;

  // quic::QuicSession::Visitor overrides
  void OnConnectionClosed(quic::QuicConnectionId server_connection_id,
                          quic::QuicErrorCode error_code,
                          const std::string& error_details,
                          quic::ConnectionCloseSource source) override;
  void OnWriteBlocked(
      quic::QuicBlockedWriterInterface* blocked_writer) override;
  void OnRstStreamReceived(const quic::QuicRstStreamFrame& frame) override;
  void OnStopSendingReceived(const quic::QuicStopSendingFrame& frame) override;
  bool TryAddNewConnectionId(
      const quic::QuicConnectionId& server_connection_id,
      const quic::QuicConnectionId& new_connection_id) override;
  void OnConnectionIdRetired(
      const quic::QuicConnectionId& server_connection_id) override;
  void OnServerPreferredAddressAvailable(
      const quic::QuicSocketAddress& server_preferred_address) override;
  void OnPathDegrading() override;

  // OpenScreenSessionBase::Visitor overrides
  void OnCryptoHandshakeComplete() override;
  void OnIncomingStream(QuicStream* QuicStream) override;
  void OnClientCertificates(const std::vector<std::string>& certs) override;
  Delegate& GetConnectionDelegate() override { return delegate_; }
  uint64_t GetInstanceID() override { return instance_id_; }

  void set_dispacher(QuicDispatcherImpl* dispatcher) {
    dispatcher_ = dispatcher;
  }

  void set_session(OpenScreenSessionBase* session, bool owns_session) {
    session_ = session;
    owns_session_ = owns_session;
  }

 private:
  const quic::QuicClock& clock_;  // Not owned.
  // `dispatcher_` is only needed for QuicServer side.
  QuicDispatcherImpl* dispatcher_ = nullptr;
  OpenScreenSessionBase* session_ = nullptr;
  // On QuicClient side, `owns_session_` is true and `session_` is owned by
  // this class.
  // On QuicServer side, `owns_session_` is false and `session_` is owned by
  // QuicDispatcherImpl.
  bool owns_session_ = false;
};

}  // namespace openscreen::osp

#endif  // OSP_IMPL_QUIC_QUIC_CONNECTION_IMPL_H_
