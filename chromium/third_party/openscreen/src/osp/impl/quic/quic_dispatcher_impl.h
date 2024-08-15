// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OSP_IMPL_QUIC_QUIC_DISPATCHER_IMPL_H_
#define OSP_IMPL_QUIC_QUIC_DISPATCHER_IMPL_H_

#include <memory>

#include "osp/impl/quic/quic_connection_factory_server.h"
#include "quiche/quic/core/quic_dispatcher.h"

namespace openscreen::osp {

class QuicDispatcherImpl : public quic::QuicDispatcher {
 public:
  QuicDispatcherImpl(
      const quic::QuicConfig* config,
      const quic::QuicCryptoServerConfig* crypto_server_config,
      std::unique_ptr<quic::QuicVersionManager> version_manager,
      std::unique_ptr<quic::QuicConnectionHelperInterface> helper,
      std::unique_ptr<quic::QuicCryptoServerStreamBase::Helper> session_helper,
      std::unique_ptr<quic::QuicAlarmFactory> alarm_factory,
      uint8_t expected_server_connection_id_length,
      quic::ConnectionIdGeneratorInterface& generator,
      QuicConnectionFactoryServer& parent_factory);
  ~QuicDispatcherImpl() override;

 protected:
  std::unique_ptr<quic::QuicSession> CreateQuicSession(
      quic::QuicConnectionId connection_id,
      const quic::QuicSocketAddress& self_address,
      const quic::QuicSocketAddress& peer_address,
      absl::string_view alpn,
      const quic::ParsedQuicVersion& version,
      const quic::ParsedClientHello& parsed_chlo,
      quic::ConnectionIdGeneratorInterface& connection_id_generator) override;

  quic::QuicDispatcher::QuicPacketFate ValidityChecksOnFullChlo(
      const quic::ReceivedPacketInfo& packet_info,
      const quic::ParsedClientHello& parsed_chlo) const override;

 private:
  std::unique_ptr<quic::QuicVersionManager> version_manager_;
  QuicConnectionFactoryServer& parent_factory_;
};

}  // namespace openscreen::osp

#endif  // OSP_IMPL_QUIC_QUIC_DISPATCHER_IMPL_H_
