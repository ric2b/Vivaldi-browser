// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp/impl/quic/quic_dispatcher_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "osp/impl/quic/open_screen_server_session.h"
#include "osp/impl/quic/quic_connection_impl.h"
#include "osp/impl/quic/quic_packet_writer_impl.h"
#include "osp/impl/quic/quic_server.h"
#include "osp/impl/quic/quic_utils.h"
#include "quiche/quic/core/crypto/quic_compressed_certs_cache.h"
#include "quiche/quic/core/crypto/quic_crypto_server_config.h"
#include "quiche/quic/core/quic_versions.h"

namespace openscreen::osp {

QuicDispatcherImpl::QuicDispatcherImpl(
    const quic::QuicConfig* config,
    const quic::QuicCryptoServerConfig* crypto_server_config,
    std::unique_ptr<quic::QuicVersionManager> version_manager,
    std::unique_ptr<quic::QuicConnectionHelperInterface> helper,
    std::unique_ptr<quic::QuicAlarmFactory> alarm_factory,
    uint8_t expected_server_connection_id_length,
    quic::ConnectionIdGeneratorInterface& generator,
    QuicConnectionFactoryServer& parent_factory)
    : QuicDispatcher(config,
                     crypto_server_config,
                     version_manager.get(),
                     std::move(helper),
                     /*session_helper*/ nullptr,
                     std::move(alarm_factory),
                     expected_server_connection_id_length,
                     generator),
      version_manager_(std::move(version_manager)),
      parent_factory_(parent_factory) {}

QuicDispatcherImpl::~QuicDispatcherImpl() = default;

std::unique_ptr<quic::QuicSession> QuicDispatcherImpl::CreateQuicSession(
    quic::QuicConnectionId connection_id,
    const quic::QuicSocketAddress& self_address,
    const quic::QuicSocketAddress& peer_address,
    absl::string_view /*alpn*/,
    const quic::ParsedQuicVersion& version,
    const quic::ParsedClientHello& /*parsed_chlo*/,
    quic::ConnectionIdGeneratorInterface& connection_id_generator) {
  auto connection = std::make_unique<quic::QuicConnection>(
      connection_id, self_address, peer_address, helper(), alarm_factory(),
      writer(), /*owns_writer*/ false, quic::Perspective::IS_SERVER,
      quic::ParsedQuicVersionVector{version}, connection_id_generator);

  auto connection_impl = std::make_unique<QuicConnectionImpl>(
      // NOTE: There is no corresponding instance name for IPEndpoint on the
      // client side. So IPEndpoint is converted into a string and used as
      // instance name.
      ToIPEndpoint(peer_address).ToString(),
      parent_factory_.server_delegate()->GetConnectionDelegate(),
      *helper()->GetClock());
  connection_impl->set_dispacher(this);

  std::unique_ptr<OpenScreenSessionBase> session =
      std::make_unique<OpenScreenServerSession>(
          std::move(connection), *connection_impl, *crypto_config(), config(),
          quic::ParsedQuicVersionVector{version});
  connection_impl->set_session(session.get(), /*owns_session*/ false);

  parent_factory_.connection().emplace(
      ToIPEndpoint(peer_address),
      QuicConnectionFactoryBase::OpenConnection{
          connection_impl.get(),
          static_cast<PacketWriterImpl*>(writer())->socket()});
  parent_factory_.server_delegate()->OnIncomingConnection(
      std::move(connection_impl));

  return session;
}

quic::QuicDispatcher::QuicPacketFate
QuicDispatcherImpl::ValidityChecksOnFullChlo(
    const quic::ReceivedPacketInfo& /*packet_info*/,
    const quic::ParsedClientHello& parsed_chlo) const {
  QuicServer* server =
      static_cast<QuicServer*>(parent_factory_.server_delegate());
  // NOTE: Use instance name + domain temporarily to prevent blocking the
  // project. There is an ongoing discussion about this, see blow link：
  // https://github.com/w3c/openscreenprotocol/issues/275
  std::string sni = server->instance_name() + ".local";
  if (sni != parsed_chlo.sni) {
    return kFateDrop;
  }

  return kFateProcess;
}

}  // namespace openscreen::osp
