// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp/impl/quic/quic_connection_factory_server.h"

#include <algorithm>
#include <string>
#include <vector>

#include "osp/impl/quic/open_screen_server_session.h"
#include "osp/impl/quic/quic_alarm_factory_impl.h"
#include "osp/impl/quic/quic_constants.h"
#include "osp/impl/quic/quic_dispatcher_impl.h"
#include "osp/impl/quic/quic_packet_writer_impl.h"
#include "osp/impl/quic/quic_server.h"
#include "osp/impl/quic/quic_utils.h"
#include "osp/impl/quic/quic_version_manager.h"
#include "quiche/quic/core/quic_default_connection_helper.h"
#include "util/osp_logging.h"
#include "util/std_util.h"
#include "util/trace_logging.h"

namespace openscreen::osp {

QuicConnectionFactoryServer::QuicConnectionFactoryServer(
    TaskRunner& task_runner)
    : QuicConnectionFactoryBase(task_runner) {}

QuicConnectionFactoryServer::~QuicConnectionFactoryServer() = default;

void QuicConnectionFactoryServer::SetServerDelegate(
    ServerDelegate* delegate,
    const std::vector<IPEndpoint>& endpoints) {
  OSP_CHECK(!delegate != !server_delegate_);

  server_delegate_ = delegate;
  dispatchers_.reserve(dispatchers_.size() + endpoints.size());

  crypto_server_config_ = std::make_unique<quic::QuicCryptoServerConfig>(
      kSourceAddressTokenSecret, quic::QuicRandom::GetInstance(),
      QuicServiceBase::GetAgentCertificate().CreateServerProofSource(),
      quic::KeyExchangeSource::Default());

  for (const auto& endpoint : endpoints) {
    // TODO(mfoltz): Need to notify the caller and/or ServerDelegate if socket
    // create/bind errors occur. Maybe return an Error immediately, and undo
    // partial progress (i.e. "unwatch" all the sockets and call
    // dispatchers_.clear() to close the sockets)?
    auto create_result = UdpSocket::Create(task_runner_, this, endpoint);
    if (!create_result) {
      OSP_LOG_ERROR << "failed to create socket (for " << endpoint
                    << "): " << create_result.error().message();
      continue;
    }
    std::unique_ptr<UdpSocket> server_socket = std::move(create_result.value());
    server_socket->Bind();

    auto version_manager =
        std::make_unique<QuicVersionManager>(supported_versions_);
    auto dispatcher = std::make_unique<QuicDispatcherImpl>(
        &config_, crypto_server_config_.get(), std::move(version_manager),
        std::make_unique<quic::QuicDefaultConnectionHelper>(),
        std::make_unique<QuicAlarmFactoryImpl>(task_runner_,
                                               quic::QuicDefaultClock::Get()),
        /*expected_server_connection_id_length=*/0u, connection_id_generator_,
        *this);
    quic::QuicPacketWriter* writer = new PacketWriterImpl(server_socket.get());
    dispatcher->InitializeWithWriter(writer);
    dispatcher->ProcessBufferedChlos(kMaxConnectionsToCreate);
    dispatchers_.emplace_back(std::move(server_socket), std::move(dispatcher));
  }
}

void QuicConnectionFactoryServer::OnRead(UdpSocket* socket,
                                         ErrorOr<UdpPacket> packet_or_error) {
  TRACE_SCOPED(TraceCategory::kQuic, "QuicConnectionFactoryServer::OnRead");
  if (packet_or_error.is_error()) {
    TRACE_SET_RESULT(packet_or_error.error());
    return;
  }

  UdpPacket packet = std::move(packet_or_error.value());
  // TODO(btolsch): We will need to rethink this both for ICE and connection
  // migration support.
  auto conn_it = connections_.find(packet.source());
  auto dispatcher_it = std::find_if(
      dispatchers_.begin(), dispatchers_.end(),
      [socket](const std::pair<std::unique_ptr<UdpSocket>,
                               std::unique_ptr<QuicDispatcherImpl>>& item) {
        return item.first.get() == socket;
      });
  QuicDispatcherImpl* dispatcher = dispatcher_it != dispatchers_.end()
                                       ? dispatcher_it->second.get()
                                       : nullptr;

  // Return early because no dispatcher can process the `packet` in this case.
  if (!dispatcher) {
    return;
  }

  std::string log_info =
      (conn_it == connections_.end())
          ? ": QuicDispatcherImpl spawns connection from "
          : ": QuicDispatcherImpl processes data for existing connection from ";
  OSP_VLOG << __func__ << log_info << packet.source();

  const quic::QuicReceivedPacket quic_packet(
      reinterpret_cast<const char*>(packet.data()), packet.size(),
      helper_->GetClock()->Now());
  dispatcher->ProcessPacket(ToQuicSocketAddress(socket->GetLocalEndpoint()),
                            ToQuicSocketAddress(packet.source()), quic_packet);
}

void QuicConnectionFactoryServer::OnConnectionClosed(
    QuicConnection* connection) {
  auto entry = std::find_if(
      connections_.begin(), connections_.end(),
      [connection](const decltype(connections_)::value_type& entry) {
        return entry.second.connection == connection;
      });

  if (entry == connections_.end()) {
    return;
  }

  UdpSocket* const socket = entry->second.socket;
  connections_.erase(entry);

  // If none of the remaining |connections_| reference the socket, close/destroy
  // it.
  if (!ContainsIf(connections_,
                  [socket](const decltype(connections_)::value_type& entry) {
                    return entry.second.socket == socket;
                  })) {
    auto socket_it = std::find_if(
        dispatchers_.begin(), dispatchers_.end(),
        [socket](const std::pair<std::unique_ptr<UdpSocket>,
                                 std::unique_ptr<QuicDispatcherImpl>>& item) {
          return item.first.get() == socket;
        });
    OSP_CHECK(socket_it != dispatchers_.end());
    dispatchers_.erase(socket_it);
  }
}

}  // namespace openscreen::osp
