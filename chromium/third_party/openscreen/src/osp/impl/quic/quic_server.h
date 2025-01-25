// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OSP_IMPL_QUIC_QUIC_SERVER_H_
#define OSP_IMPL_QUIC_QUIC_SERVER_H_

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "osp/impl/quic/certificates/quic_agent_certificate.h"
#include "osp/impl/quic/quic_connection_factory_server.h"
#include "osp/impl/quic/quic_service_base.h"
#include "osp/public/protocol_connection_server.h"

namespace openscreen::osp {

// This class is the default implementation of ProtocolConnectionServer for the
// library.  It manages connections to other endpoints as well as the lifetime
// of each incoming and outgoing stream.  It works in conjunction with a
// QuicConnectionFactoryServer and MessageDemuxer.
// QuicConnectionFactoryServer provides the ability to make a new QUIC
// connection from packets received on its server sockets.  Incoming data is
// given to the QuicServer by the underlying QUIC implementation (through
// QuicConnectionFactoryServer) and this is in turn handed to MessageDemuxer for
// routing CBOR messages.
class QuicServer final : public ProtocolConnectionServer,
                         public QuicConnectionFactoryServer::ServerDelegate,
                         public QuicServiceBase {
 public:
  static QuicAgentCertificate& GetAgentCertificate();

  QuicServer(const ServiceConfig& config,
             MessageDemuxer& demuxer,
             std::unique_ptr<QuicConnectionFactoryServer> connection_factory,
             ProtocolConnectionServiceObserver& observer,
             ClockNowFunctionPtr now_function,
             TaskRunner& task_runner);
  QuicServer(const QuicServer&) = delete;
  QuicServer& operator=(const QuicServer&) = delete;
  QuicServer(QuicServer&&) noexcept = delete;
  QuicServer& operator=(QuicServer&&) noexcept = delete;
  ~QuicServer() override;

  // ProtocolConnectionServer overrides.
  bool Start() override;
  bool Stop() override;
  bool Suspend() override;
  bool Resume() override;
  State GetState() override;
  MessageDemuxer& GetMessageDemuxer() override;
  InstanceRequestIds& GetInstanceRequestIds() override;
  std::unique_ptr<ProtocolConnection> CreateProtocolConnection(
      uint64_t instance_id) override;
  std::string GetAgentFingerprint() override;

  // QuicServiceBase overrides.
  uint64_t OnCryptoHandshakeComplete(std::string_view instance_name) override;
  void OnConnectionClosed(uint64_t instance_id) override;

  const std::string& instance_name() const { return instance_name_; }

 private:
  // QuicServiceBase overrides.
  void CloseAllConnections() override;

  // QuicConnectionFactoryServer::ServerDelegate overrides.
  QuicConnection::Delegate& GetConnectionDelegate() override { return *this; }
  void OnIncomingConnection(
      std::unique_ptr<QuicConnection> connection) override;

  // This is used for server name indication check.
  const std::string instance_name_;

  std::unique_ptr<QuicConnectionFactoryServer> connection_factory_;

  // Maps an instance name to data about connections that haven't successfully
  // completed the QUIC handshake.
  std::map<std::string, ServiceConnectionData, std::less<>>
      pending_connections_;
};

}  // namespace openscreen::osp

#endif  // OSP_IMPL_QUIC_QUIC_SERVER_H_
