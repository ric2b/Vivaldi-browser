// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OSP_IMPL_QUIC_TESTING_QUIC_TEST_SUPPORT_H_
#define OSP_IMPL_QUIC_TESTING_QUIC_TEST_SUPPORT_H_

#include <memory>
#include <string>

#include "gmock/gmock.h"
#include "osp/impl/quic/quic_client.h"
#include "osp/impl/quic/quic_server.h"
#include "osp/impl/quic/testing/fake_quic_connection_factory.h"
#include "osp/public/network_metrics.h"
#include "osp/public/network_service_manager.h"
#include "osp/public/protocol_connection_client.h"
#include "osp/public/protocol_connection_server.h"
#include "osp/public/protocol_connection_service_observer.h"
#include "platform/api/time.h"
#include "platform/api/udp_socket.h"
#include "platform/base/ip_address.h"
#include "platform/test/fake_clock.h"
#include "platform/test/fake_task_runner.h"
#include "platform/test/fake_udp_socket.h"

namespace openscreen::osp {

class MockServiceObserver : public ProtocolConnectionServiceObserver {
 public:
  ~MockServiceObserver() override = default;

  MOCK_METHOD0(OnRunning, void());
  MOCK_METHOD0(OnStopped, void());
  MOCK_METHOD0(OnSuspended, void());

  MOCK_METHOD1(OnMetrics, void(const NetworkMetrics& metrics));
  MOCK_METHOD1(OnError, void(const Error& error));

  void OnIncomingConnection(
      std::unique_ptr<ProtocolConnection> connection) override {
    OnIncomingConnectionMock(connection);
  }
  MOCK_METHOD1(OnIncomingConnectionMock,
               void(std::unique_ptr<ProtocolConnection>& connection));
};

class FakeQuicBridge {
 public:
  FakeQuicBridge(FakeTaskRunner& task_runner, ClockNowFunctionPtr now_function);
  ~FakeQuicBridge();

  const IPEndpoint kControllerEndpoint{{192, 168, 1, 3}, 4321};
  const IPEndpoint kReceiverEndpoint{{192, 168, 1, 17}, 1234};
  const std::string kInstanceName{"test instance name"};
  const std::string kFingerprint{"test fringprint"};
  const std::string kAuthToken{"test token"};

  void CreateNetworkServiceManager(
      std::unique_ptr<ServiceListener> service_listener,
      std::unique_ptr<ServicePublisher> service_publisher);
  QuicClient* GetQuicClient();
  QuicServer* GetQuicServer();
  MessageDemuxer& GetControllerDemuxer();
  MessageDemuxer& GetReceiverDemuxer();
  ::testing::NiceMock<MockServiceObserver>& mock_client_observer() {
    return mock_client_observer_;
  }
  ::testing::NiceMock<MockServiceObserver>& mock_server_observer() {
    return mock_server_observer_;
  }

  void RunTasksUntilIdle();

 private:
  void PostClientPacket();
  void PostServerPacket();
  void PostPacketsUntilIdle();

  // Indicate if this is used together with a NetworkServiceManager. If true, it
  // means QuicClient and QuicServer are owned by NetworkServiceManager.
  // Otherwise, they are owned by this class.
  bool use_network_service_manager_ = false;
  FakeTaskRunner& task_runner_;
  std::unique_ptr<QuicClient> quic_client_;
  std::unique_ptr<QuicServer> quic_server_;
  std::unique_ptr<FakeQuicConnectionFactoryBridge> fake_bridge_;
  ::testing::NiceMock<MockServiceObserver> mock_client_observer_;
  ::testing::NiceMock<MockServiceObserver> mock_server_observer_;
  std::unique_ptr<FakeUdpSocket> client_socket_;
  std::unique_ptr<FakeUdpSocket> server_socket_;
};

}  // namespace openscreen::osp

#endif  // OSP_IMPL_QUIC_TESTING_QUIC_TEST_SUPPORT_H_
