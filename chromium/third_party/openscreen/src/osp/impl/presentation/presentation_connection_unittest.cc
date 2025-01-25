// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp/public/presentation/presentation_connection.h"

#include <memory>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "osp/impl/presentation/presentation_utils.h"
#include "osp/impl/presentation/testing/mock_connection_delegate.h"
#include "osp/impl/quic/testing/fake_quic_connection.h"
#include "osp/impl/quic/testing/fake_quic_connection_factory.h"
#include "osp/impl/quic/testing/quic_test_support.h"
#include "osp/public/connect_request.h"
#include "osp/public/network_service_manager.h"
#include "osp/public/presentation/presentation_controller.h"
#include "platform/test/fake_clock.h"
#include "platform/test/fake_task_runner.h"

namespace openscreen::osp {

using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;

namespace {

class MockController : public Connection::Controller {
 public:
  MockController() = default;
  ~MockController() override = default;

  MOCK_METHOD2(CloseConnection, Error(Connection*, Connection::CloseReason));
  MOCK_METHOD3(OnPresentationTerminated,
               Error(const std::string&, TerminationSource, TerminationReason));
  MOCK_METHOD1(OnConnectionDestroyed, void(Connection*));
};

class MockConnectRequestCallback final : public ConnectRequestCallback {
 public:
  ~MockConnectRequestCallback() override = default;

  MOCK_METHOD2(OnConnectSucceed,
               void(uint64_t request_id, uint64_t instance_id));
  MOCK_METHOD1(OnConnectFailed, void(uint64_t request_id));
};

}  // namespace

class ConnectionTest : public ::testing::Test {
 public:
  ConnectionTest()
      : fake_clock_(Clock::time_point(std::chrono::milliseconds(1298424))),
        task_runner_(fake_clock_),
        quic_bridge_(task_runner_, FakeClock::now),
        controller_connection_manager_(quic_bridge_.GetControllerDemuxer()),
        receiver_connection_manager_(quic_bridge_.GetReceiverDemuxer()) {}

 protected:
  void SetUp() override {
    quic_bridge_.CreateNetworkServiceManager(nullptr, nullptr);
  }

  std::string MakeEchoResponse(const std::string& message) {
    return std::string("echo: ") + message;
  }

  std::vector<uint8_t> MakeEchoResponse(const std::vector<uint8_t>& data) {
    std::vector<uint8_t> response{13, 14, 15};
    response.insert(response.end(), data.begin(), data.end());
    return response;
  }

  FakeClock fake_clock_;
  FakeTaskRunner task_runner_;
  FakeQuicBridge quic_bridge_;
  ConnectionManager controller_connection_manager_;
  ConnectionManager receiver_connection_manager_;
  NiceMock<MockController> mock_controller_;
  NiceMock<MockController> mock_receiver_;
};

TEST_F(ConnectionTest, ConnectAndSend) {
  const std::string id{"deadbeef01234"};
  const std::string url{"https://example.com/receiver.html"};
  const uint64_t connection_id = 13;
  MockConnectionDelegate mock_controller_delegate;
  MockConnectionDelegate mock_receiver_delegate;
  Connection controller(Connection::PresentationInfo{id, url},
                        &mock_controller_delegate, &mock_controller_);
  Connection receiver(Connection::PresentationInfo{id, url},
                      &mock_receiver_delegate, &mock_receiver_);
  ON_CALL(mock_controller_, OnPresentationTerminated(_, _, _))
      .WillByDefault(Invoke([&receiver](const std::string& presentation_id,
                                        TerminationSource source,
                                        TerminationReason reason) {
        receiver.OnTerminated();
        return Error::None();
      }));
  ON_CALL(mock_controller_, CloseConnection(_, _))
      .WillByDefault(Invoke(
          [&receiver](Connection* connection, Connection::CloseReason reason) {
            receiver.OnClosedByRemote();
            return Error::None();
          }));
  ON_CALL(mock_receiver_, OnPresentationTerminated(_, _, _))
      .WillByDefault(Invoke([&controller](const std::string& presentation_id,
                                          TerminationSource source,
                                          TerminationReason reason) {
        controller.OnTerminated();
        return Error::None();
      }));
  ON_CALL(mock_receiver_, CloseConnection(_, _))
      .WillByDefault(Invoke([&controller](Connection* connection,
                                          Connection::CloseReason reason) {
        controller.OnClosedByRemote();
        return Error::None();
      }));

  EXPECT_EQ(id, controller.presentation_info().id);
  EXPECT_EQ(url, controller.presentation_info().url);
  EXPECT_EQ(id, receiver.presentation_info().id);
  EXPECT_EQ(url, receiver.presentation_info().url);

  EXPECT_EQ(Connection::State::kConnecting, controller.state());
  EXPECT_EQ(Connection::State::kConnecting, receiver.state());

  MockConnectRequestCallback mock_connect_request_callback;
  ConnectRequest request;
  std::unique_ptr<ProtocolConnection> controller_stream;
  std::unique_ptr<ProtocolConnection> receiver_stream;
  quic_bridge_.GetQuicClient()->Connect(quic_bridge_.kInstanceName, request,
                                        &mock_connect_request_callback);
  EXPECT_TRUE(request);
  EXPECT_CALL(mock_connect_request_callback, OnConnectSucceed(_, _))
      .WillOnce(Invoke(
          [&controller_stream](uint64_t request_id, uint64_t instance_id) {
            controller_stream = CreateClientProtocolConnection(instance_id);
          }));

  EXPECT_CALL(quic_bridge_.mock_server_observer(), OnIncomingConnectionMock(_))
      .WillOnce(testing::WithArgs<0>(testing::Invoke(
          [&receiver_stream](std::unique_ptr<ProtocolConnection>& connection) {
            receiver_stream = std::move(connection);
          })));

  quic_bridge_.RunTasksUntilIdle();
  ASSERT_TRUE(controller_stream);
  ASSERT_TRUE(receiver_stream);

  EXPECT_CALL(mock_controller_delegate, OnConnected());
  EXPECT_CALL(mock_receiver_delegate, OnConnected());
  uint64_t controller_instance_id = receiver_stream->GetInstanceID();
  uint64_t receiver_instance_id = controller_stream->GetInstanceID();
  controller.OnConnected(connection_id, receiver_instance_id,
                         std::move(controller_stream));
  receiver.OnConnected(connection_id, controller_instance_id,
                       std::move(receiver_stream));
  controller_connection_manager_.AddConnection(&controller);
  receiver_connection_manager_.AddConnection(&receiver);

  EXPECT_EQ(Connection::State::kConnected, controller.state());
  EXPECT_EQ(Connection::State::kConnected, receiver.state());

  std::string message = "some connection message";
  const std::string expected_message = message;
  const std::string expected_response = MakeEchoResponse(expected_message);

  controller.SendString(message);

  std::string received;
  EXPECT_CALL(mock_receiver_delegate,
              OnStringMessage(static_cast<std::string_view>(expected_message)))
      .WillOnce(Invoke(
          [&received](std::string_view s) { received = std::string(s); }));
  quic_bridge_.RunTasksUntilIdle();

  std::string string_response = MakeEchoResponse(received);
  receiver.SendString(string_response);

  EXPECT_CALL(
      mock_controller_delegate,
      OnStringMessage(static_cast<std::string_view>(expected_response)));
  quic_bridge_.RunTasksUntilIdle();

  std::vector<uint8_t> data{0, 3, 2, 4, 4, 6, 1};
  const std::vector<uint8_t> expected_data = data;
  const std::vector<uint8_t> expected_response_data =
      MakeEchoResponse(expected_data);

  controller.SendBinary(std::move(data));

  std::vector<uint8_t> received_data;
  EXPECT_CALL(mock_receiver_delegate, OnBinaryMessage(expected_data))
      .WillOnce(Invoke([&received_data](std::vector<uint8_t> d) {
        received_data = std::move(d);
      }));
  quic_bridge_.RunTasksUntilIdle();

  receiver.SendBinary(MakeEchoResponse(received_data));
  EXPECT_CALL(mock_controller_delegate,
              OnBinaryMessage(expected_response_data));
  quic_bridge_.RunTasksUntilIdle();

  EXPECT_CALL(mock_controller_delegate, OnClosedByRemote());
  receiver.Close(Connection::CloseReason::kClosed);
  quic_bridge_.RunTasksUntilIdle();
  EXPECT_EQ(Connection::State::kClosed, controller.state());
  EXPECT_EQ(Connection::State::kClosed, receiver.state());
  controller_connection_manager_.RemoveConnection(&controller);
  receiver_connection_manager_.RemoveConnection(&receiver);
}

}  // namespace openscreen::osp
