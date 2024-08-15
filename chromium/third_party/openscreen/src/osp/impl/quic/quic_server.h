// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OSP_IMPL_QUIC_QUIC_SERVER_H_
#define OSP_IMPL_QUIC_QUIC_SERVER_H_

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "osp/impl/quic/quic_connection_factory_server.h"
#include "osp/impl/quic/quic_service_common.h"
#include "osp/public/endpoint_config.h"
#include "osp/public/protocol_connection_server.h"
#include "platform/api/task_runner.h"
#include "platform/api/time.h"
#include "platform/base/ip_address.h"
#include "util/alarm.h"

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
                         public ServiceConnectionDelegate::ServiceDelegate {
 public:
  QuicServer(const EndpointConfig& config,
             MessageDemuxer& demuxer,
             std::unique_ptr<QuicConnectionFactoryServer> connection_factory,
             ProtocolConnectionServer::Observer& observer,
             ClockNowFunctionPtr now_function,
             TaskRunner& task_runner);
  ~QuicServer() override;

  // ProtocolConnectionServer overrides.
  bool Start() override;
  bool Stop() override;
  bool Suspend() override;
  bool Resume() override;
  std::string GetFingerprint() override;
  std::unique_ptr<ProtocolConnection> CreateProtocolConnection(
      uint64_t endpoint_id) override;

  // QuicProtocolConnection::Owner overrides.
  void OnConnectionDestroyed(QuicProtocolConnection* connection) override;

  // ServiceConnectionDelegate::ServiceDelegate overrides.
  uint64_t OnCryptoHandshakeComplete(ServiceConnectionDelegate* delegate,
                                     std::string connection_id) override;
  void OnIncomingStream(
      std::unique_ptr<QuicProtocolConnection> connection) override;
  void OnConnectionClosed(uint64_t endpoint_id,
                          std::string connection_id) override;
  void OnDataReceived(uint64_t endpoint_id,
                      uint64_t protocol_connection_id,
                      const ByteView& bytes) override;

 private:
  void CloseAllConnections();

  // QuicConnectionFactoryServer::ServerDelegate overrides.
  QuicConnection::Delegate* NextConnectionDelegate(
      const IPEndpoint& source) override;
  void OnIncomingConnection(
      std::unique_ptr<QuicConnection> connection) override;

  // Deletes dead QUIC connections then returns the time interval before this
  // method should be run again.
  void Cleanup();

  const std::vector<IPEndpoint> connection_endpoints_;
  std::unique_ptr<QuicConnectionFactoryServer> connection_factory_;

  std::unique_ptr<ServiceConnectionDelegate> pending_connection_delegate_;

  // Maps an IPEndpoint to a generated endpoint ID.  This is used to insulate
  // callers from post-handshake changes to a connections actual peer endpoint.
  std::map<IPEndpoint, uint64_t> endpoint_map_;

  // Value that will be used for the next new endpoint in a Connect call.
  uint64_t next_endpoint_id_ = 0;

  // Maps endpoint addresses to data about connections that haven't successfully
  // completed the QUIC handshake.
  std::map<IPEndpoint, ServiceConnectionData> pending_connections_;

  // Maps endpoint IDs to data about connections that have successfully
  // completed the QUIC handshake.
  std::map<uint64_t, ServiceConnectionData> connections_;

  // Connections (endpoint IDs) that need to be destroyed, but have to wait for
  // the next event loop due to the underlying QUIC implementation's way of
  // referencing them.
  std::vector<uint64_t> delete_connections_;

  Alarm cleanup_alarm_;
};

}  // namespace openscreen::osp

#endif  // OSP_IMPL_QUIC_QUIC_SERVER_H_
