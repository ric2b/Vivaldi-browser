// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OSP_IMPL_QUIC_QUIC_CLIENT_H_
#define OSP_IMPL_QUIC_QUIC_CLIENT_H_

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "osp/impl/quic/quic_connection_factory_client.h"
#include "osp/impl/quic/quic_service_base.h"
#include "osp/public/protocol_connection_client.h"

namespace openscreen::osp {

// This class is the default implementation of ProtocolConnectionClient for the
// library.  It manages connections to other endpoints as well as the lifetime
// of each incoming and outgoing stream.  It works in conjunction with a
// QuicConnectionFactoryClient and MessageDemuxer.
// QuicConnectionFactoryClient provides the actual ability to make a new QUIC
// connection with another endpoint.  Incoming data is given to the QuicClient
// by the underlying QUIC implementation (through QuicConnectionFactoryClient)
// and this is in turn handed to MessageDemuxer for routing CBOR messages.
//
// The two most significant methods of this class are Connect and
// CreateProtocolConnection.  Both will return a new QUIC stream to a given
// endpoint to which the caller can write but the former is allowed to be
// asynchronous.  If there isn't currently a connection to the specified
// endpoint, Connect will start a connection attempt and store the callback for
// when the connection completes.  CreateProtocolConnection simply returns
// nullptr if there's no existing connection.
class QuicClient final : public ProtocolConnectionClient,
                         public QuicServiceBase {
 public:
  QuicClient(const ServiceConfig& config,
             std::unique_ptr<QuicConnectionFactoryClient> connection_factory,
             ProtocolConnectionServiceObserver& observer,
             ClockNowFunctionPtr now_function,
             TaskRunner& task_runner,
             size_t buffer_limit);
  QuicClient(const QuicClient&) = delete;
  QuicClient& operator=(const QuicClient&) = delete;
  QuicClient(QuicClient&&) noexcept = delete;
  QuicClient& operator=(QuicClient&&) noexcept = delete;
  ~QuicClient() override;

  // ProtocolConnectionClient overrides.
  bool Start() override;
  bool Stop() override;
  bool Suspend() override;
  bool Resume() override;
  State GetState() override { return state_; }
  MessageDemuxer& GetMessageDemuxer() override { return demuxer_; }
  InstanceRequestIds& GetInstanceRequestIds() override {
    return instance_request_ids_;
  }
  std::unique_ptr<ProtocolConnection> CreateProtocolConnection(
      uint64_t instance_id) override;
  bool Connect(std::string_view instance_name,
               ConnectRequest& request,
               ConnectRequestCallback* request_callback) override;

 private:
  // FakeQuicBridge needs to access `instance_infos_` and struct InstanceInfo
  // for tests.
  friend class FakeQuicBridge;

  // This struct holds necessary information of an instance found through
  // discovery for building connection and authentication.
  struct InstanceInfo {
    // Agent fingerprint.
    std::string fingerprint;

    // Token published by the other agent that allows this agent to initiate
    // authentication with that agent.
    std::string auth_token;

    // The network endpoints to create a new connection to the Open Screen
    // service. At least one of them is valid and use `v4_endpoint` first if it
    // is valid.
    IPEndpoint v4_endpoint;
    IPEndpoint v6_endpoint;
  };

  // ServiceListener::Observer overrides.
  void OnStarted() override;
  void OnStopped() override;
  void OnSuspended() override;
  void OnSearching() override;
  void OnReceiverAdded(const ServiceInfo& info) override;
  void OnReceiverChanged(const ServiceInfo& info) override;
  void OnReceiverRemoved(const ServiceInfo& info) override;
  void OnAllReceiversRemoved() override;
  void OnError(const Error& error) override;
  void OnMetrics(ServiceListener::Metrics) override;

  bool StartConnectionRequest(std::string_view instance_name,
                              ConnectRequest& request,
                              ConnectRequestCallback* request_callback);
  void CancelConnectRequest(uint64_t request_id) override;

  // Value that will be used for the next new connection request.
  uint64_t next_request_id_ = 1u;

  // Maps an instance name to necessary information of the instance used to
  // build connection.
  std::map<std::string, InstanceInfo, std::less<>> instance_infos_;
};

}  // namespace openscreen::osp

#endif  // OSP_IMPL_QUIC_QUIC_CLIENT_H_
