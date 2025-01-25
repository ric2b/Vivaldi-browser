// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OSP_IMPL_QUIC_QUIC_SERVICE_BASE_H_
#define OSP_IMPL_QUIC_QUIC_SERVICE_BASE_H_

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "osp/impl/quic/certificates/quic_agent_certificate.h"
#include "osp/impl/quic/quic_connection_factory_base.h"
#include "osp/impl/quic/quic_stream_manager.h"
#include "osp/public/connect_request.h"
#include "osp/public/instance_request_ids.h"
#include "osp/public/message_demuxer.h"
#include "osp/public/protocol_connection_endpoint.h"
#include "osp/public/protocol_connection_service_observer.h"
#include "osp/public/service_config.h"
#include "platform/api/task_runner.h"
#include "platform/api/time.h"
#include "platform/base/ip_address.h"
#include "util/alarm.h"

namespace openscreen::osp {

// There are two kinds of QUIC services: QuicServer and QuicClient. They differ
// in the connection establishment process, but they share much of the same
// logic. This class holds common codes for the two classes.
class QuicServiceBase : public QuicConnection::Delegate,
                        public QuicStreamManager::Delegate {
 public:
  static QuicAgentCertificate& GetAgentCertificate();

  QuicServiceBase(const ServiceConfig& config,
                  std::unique_ptr<QuicConnectionFactoryBase> connection_factory,
                  ProtocolConnectionServiceObserver& observer,
                  InstanceRequestIds::Role role,
                  ClockNowFunctionPtr now_function,
                  TaskRunner& task_runner,
                  size_t buffer_limit);
  QuicServiceBase(const QuicServiceBase&) = delete;
  QuicServiceBase& operator=(const QuicServiceBase&) = delete;
  QuicServiceBase(QuicServiceBase&&) noexcept = delete;
  QuicServiceBase& operator=(QuicServiceBase&&) noexcept = delete;
  virtual ~QuicServiceBase();

  // QuicConnection::Delegate overrides.
  uint64_t OnCryptoHandshakeComplete(std::string_view instance_name) override;
  void OnIncomingStream(uint64_t instance_id, QuicStream* stream) override;
  void OnConnectionClosed(uint64_t instance_id) override;
  QuicStream::Delegate& GetStreamDelegate(uint64_t instance_id) override;
  void OnClientCertificates(std::string_view instance_name,
                            const std::vector<std::string>& certs) override;

  // QuicStreamManager::Delegate overrides.
  void OnConnectionDestroyed(QuicProtocolConnection& connection) override;
  void OnDataReceived(uint64_t instance_id,
                      uint64_t protocol_connection_id,
                      ByteView bytes) override;
  void OnClose(uint64_t instance_id, uint64_t protocol_connection_id) override;

 protected:
  struct ServiceConnectionData {
    ServiceConnectionData(std::unique_ptr<QuicConnection> connection,
                          std::unique_ptr<QuicStreamManager> manager);
    ServiceConnectionData(const ServiceConnectionData&) = delete;
    ServiceConnectionData& operator=(const ServiceConnectionData&) = delete;
    ServiceConnectionData(ServiceConnectionData&&) noexcept;
    ServiceConnectionData& operator=(ServiceConnectionData&&) noexcept;
    ~ServiceConnectionData();

    std::unique_ptr<QuicConnection> connection;
    std::unique_ptr<QuicStreamManager> stream_manager;
  };

  struct PendingConnectionData {
    explicit PendingConnectionData(ServiceConnectionData&& data);
    PendingConnectionData(const PendingConnectionData&) = delete;
    PendingConnectionData& operator=(const PendingConnectionData&) = delete;
    PendingConnectionData(PendingConnectionData&&) noexcept;
    PendingConnectionData& operator=(PendingConnectionData&&) noexcept;
    ~PendingConnectionData();

    ServiceConnectionData data;

    // Pairs of request IDs and the associated ConnectRequestCallback.
    // This is only used by QuicClient and is empty for QuicServer.
    std::vector<std::pair<uint64_t, ConnectRequestCallback*>> callbacks;
  };

  bool StartImpl();
  bool StopImpl();
  bool SuspendImpl();
  bool ResumeImpl();
  std::unique_ptr<ProtocolConnection> CreateProtocolConnectionImpl(
      uint64_t instance_id);

  ProtocolConnectionEndpoint::State state_ =
      ProtocolConnectionEndpoint::State::kStopped;
  InstanceRequestIds instance_request_ids_;
  MessageDemuxer demuxer_;
  std::unique_ptr<QuicConnectionFactoryBase> connection_factory_;

  // IPEndpoints used by this service to build connection.
  //
  // NOTE: QuicServer uses all IPEndpoints to build UdpSockets for listening
  // incoming connections. However, QuicClient only uses the first IPEndpoint to
  // build connections. A better way is needed to handle multiple IPEndpoints
  // situations.
  const std::vector<IPEndpoint> connection_endpoints_;

  // Map an instance name to a generated instance ID. An instance is identified
  // by instance name before connection is built and is identified by instance
  // ID for simplicity after then. See OnCryptoHandshakeComplete method. This is
  // used to insulate callers from post-handshake changes to a connections
  // actual peer instance.
  //
  // TODO(crbug.com/347268871): Replace instance_name as an agent identifier.
  std::map<std::string, uint64_t, std::less<>> instance_map_;

  // Maps an instance name to data about connections that haven't successfully
  // completed the QUIC handshake.
  std::map<std::string, PendingConnectionData, std::less<>>
      pending_connections_;

 private:
  void CloseAllConnections();

  // Delete dead QUIC connections and schedule the next call to this function.
  void Cleanup();

  // Value that will be used for the next new instance.
  uint64_t next_instance_id_ = 1u;

  // Map an instance ID to data about connections that have successfully
  // completed the QUIC handshake.
  std::map<uint64_t, ServiceConnectionData> connections_;

  // Connections (instance IDs) that need to be destroyed, but have to wait
  // for the next event loop due to the underlying QUIC implementation's way of
  // referencing them.
  std::vector<uint64_t> delete_connections_;

  ProtocolConnectionServiceObserver& observer_;
  Alarm cleanup_alarm_;
};

}  // namespace openscreen::osp

#endif  // OSP_IMPL_QUIC_QUIC_SERVICE_BASE_H_
