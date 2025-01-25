// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp/impl/quic/quic_client.h"

#include <memory>
#include <string>
#include <utility>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "osp/impl/quic/testing/fake_quic_connection_factory.h"
#include "osp/impl/quic/testing/quic_test_support.h"
#include "osp/public/connect_request.h"
#include "osp/public/network_metrics.h"
#include "osp/public/network_service_manager.h"
#include "osp/public/testing/message_demuxer_test_support.h"
#include "platform/base/error.h"
#include "platform/base/span.h"
#include "platform/test/fake_clock.h"
#include "platform/test/fake_task_runner.h"
#include "util/osp_logging.h"

namespace openscreen::osp {
namespace {

using ::testing::_;
using ::testing::Invoke;

class MockConnectionObserver final : public ProtocolConnection::Observer {
 public:
  ~MockConnectionObserver() override = default;

  MOCK_METHOD1(OnConnectionClosed, void(const ProtocolConnection& connection));
};

class ConnectCallback final : public ConnectRequestCallback {
 public:
  ConnectCallback() = default;
  ~ConnectCallback() override = default;

  void OnConnectSucceed(uint64_t request_id, uint64_t instance_id) override {
    OSP_CHECK(!failed_ && !instance_id_);
    instance_id_ = instance_id;
  }

  void OnConnectFailed(uint64_t request_id) override {
    OSP_CHECK(!failed_ && !instance_id_);
    failed_ = true;
  }

  uint64_t instance_id() { return instance_id_; }

 private:
  bool failed_ = false;
  uint64_t instance_id_ = 0u;
};

class QuicClientTest : public ::testing::Test {
 public:
  QuicClientTest()
      : fake_clock_(Clock::time_point(std::chrono::milliseconds(1298424))),
        task_runner_(fake_clock_),
        quic_bridge_(task_runner_, FakeClock::now) {}

 protected:
  void SetUp() override {
    client_ = quic_bridge_.GetQuicClient();
    quic_bridge_.CreateNetworkServiceManager(nullptr, nullptr);
    ON_CALL(quic_bridge_.mock_server_observer(), OnIncomingConnectionMock(_))
        .WillByDefault(
            Invoke([this](std::unique_ptr<ProtocolConnection>& connection) {
              server_connections_.push_back(std::move(connection));
            }));
  }

  void SendTestMessage(ProtocolConnection* connection) {
    MockMessageCallback mock_message_callback;
    MessageDemuxer::MessageWatch message_watch =
        quic_bridge_.GetReceiverDemuxer().WatchMessageType(
            1, msgs::Type::kPresentationConnectionMessage,
            &mock_message_callback);

    msgs::CborEncodeBuffer buffer;
    msgs::PresentationConnectionMessage message;
    message.connection_id = 7;
    message.message.which = decltype(message.message.which)::kString;
    new (&message.message.str) std::string("message from client");
    ASSERT_TRUE(msgs::EncodePresentationConnectionMessage(message, &buffer));
    connection->Write(ByteView(buffer.data(), buffer.size()));

    msgs::CborResult decode_result = 0;
    msgs::PresentationConnectionMessage received_message;
    EXPECT_CALL(
        mock_message_callback,
        OnStreamMessage(1, connection->GetID(),
                        msgs::Type::kPresentationConnectionMessage, _, _, _))
        .WillOnce(Invoke([&decode_result, &received_message](
                             uint64_t instance_id, uint64_t connection_id,
                             msgs::Type message_type, const uint8_t* b,
                             size_t buffer_size, Clock::time_point now) {
          decode_result = msgs::DecodePresentationConnectionMessage(
              b, buffer_size, received_message);
          if (decode_result < 0)
            return ErrorOr<size_t>(Error::Code::kCborParsing);
          return ErrorOr<size_t>(decode_result);
        }));
    quic_bridge_.RunTasksUntilIdle();

    ASSERT_GT(decode_result, 0);
    EXPECT_EQ(decode_result, static_cast<int64_t>(buffer.size() - 1));
    EXPECT_EQ(received_message.connection_id, message.connection_id);
    ASSERT_EQ(received_message.message.which,
              decltype(received_message.message.which)::kString);
    EXPECT_EQ(received_message.message.str, message.message.str);
  }

  FakeClock fake_clock_;
  FakeTaskRunner task_runner_;
  FakeQuicBridge quic_bridge_;
  QuicClient* client_;
  std::vector<std::unique_ptr<ProtocolConnection>> server_connections_;
};

}  // namespace

TEST_F(QuicClientTest, Connect) {
  client_->Start();

  ConnectCallback connection_callback;
  ConnectRequest request;
  bool result = client_->Connect(quic_bridge_.kInstanceName, request,
                                 &connection_callback);
  ASSERT_TRUE(result);
  ASSERT_TRUE(request);

  quic_bridge_.RunTasksUntilIdle();
  ASSERT_TRUE(connection_callback.instance_id());

  std::unique_ptr<ProtocolConnection> connection =
      client_->CreateProtocolConnection(connection_callback.instance_id());
  ASSERT_TRUE(connection);
  SendTestMessage(connection.get());

  client_->Stop();
}

TEST_F(QuicClientTest, DoubleConnect) {
  client_->Start();

  ConnectCallback connection_callback1;
  ConnectRequest request1;
  bool result = client_->Connect(quic_bridge_.kInstanceName, request1,
                                 &connection_callback1);
  ASSERT_TRUE(result);
  ASSERT_TRUE(request1);
  ASSERT_FALSE(connection_callback1.instance_id());

  ConnectCallback connection_callback2;
  ConnectRequest request2;
  result = client_->Connect(quic_bridge_.kInstanceName, request2,
                            &connection_callback2);
  ASSERT_TRUE(result);
  ASSERT_TRUE(request2);

  quic_bridge_.RunTasksUntilIdle();
  ASSERT_TRUE(connection_callback1.instance_id());
  ASSERT_TRUE(connection_callback2.instance_id());

  std::unique_ptr<ProtocolConnection> connection =
      client_->CreateProtocolConnection(connection_callback1.instance_id());
  ASSERT_TRUE(connection);

  SendTestMessage(connection.get());

  client_->Stop();
}

TEST_F(QuicClientTest, OpenImmediate) {
  client_->Start();

  std::unique_ptr<ProtocolConnection> connection =
      client_->CreateProtocolConnection(1);
  EXPECT_FALSE(connection);

  ConnectCallback connection_callback;
  ConnectRequest request;
  bool result = client_->Connect(quic_bridge_.kInstanceName, request,
                                 &connection_callback);
  ASSERT_TRUE(result);
  ASSERT_TRUE(request);

  connection = client_->CreateProtocolConnection(1);
  EXPECT_FALSE(connection);

  quic_bridge_.RunTasksUntilIdle();
  ASSERT_TRUE(connection_callback.instance_id());

  connection =
      client_->CreateProtocolConnection(connection_callback.instance_id());
  ASSERT_TRUE(connection);

  SendTestMessage(connection.get());

  client_->Stop();
}

TEST_F(QuicClientTest, States) {
  client_->Stop();
  ConnectCallback connection_callback1;
  ConnectRequest request1;
  bool result = client_->Connect(quic_bridge_.kInstanceName, request1,
                                 &connection_callback1);
  EXPECT_FALSE(result);
  EXPECT_FALSE(request1);

  std::unique_ptr<ProtocolConnection> connection =
      client_->CreateProtocolConnection(1);
  EXPECT_FALSE(connection);

  EXPECT_CALL(quic_bridge_.mock_client_observer(), OnRunning());
  EXPECT_TRUE(client_->Start());
  EXPECT_FALSE(client_->Start());

  ConnectCallback connection_callback2;
  ConnectRequest request2;
  result = client_->Connect(quic_bridge_.kInstanceName, request2,
                            &connection_callback2);
  ASSERT_TRUE(result);
  ASSERT_TRUE(request2);

  quic_bridge_.RunTasksUntilIdle();
  ASSERT_TRUE(connection_callback2.instance_id());

  MockConnectionObserver mock_connection_observer1;
  connection =
      client_->CreateProtocolConnection(connection_callback2.instance_id());
  ASSERT_TRUE(connection);
  connection->SetObserver(&mock_connection_observer1);

  MockConnectionObserver mock_connection_observer2;
  std::unique_ptr<ProtocolConnection> connection2 =
      client_->CreateProtocolConnection(connection_callback2.instance_id());
  ASSERT_TRUE(connection2);
  connection2->SetObserver(&mock_connection_observer2);
  EXPECT_CALL(mock_connection_observer1, OnConnectionClosed(_));
  EXPECT_CALL(mock_connection_observer2, OnConnectionClosed(_));
  EXPECT_CALL(quic_bridge_.mock_client_observer(), OnStopped());
  EXPECT_TRUE(client_->Stop());
  EXPECT_FALSE(client_->Stop());

  ConnectCallback connection_callback3;
  ConnectRequest request3;
  result = client_->Connect(quic_bridge_.kInstanceName, request3,
                            &connection_callback3);
  EXPECT_FALSE(result);
  EXPECT_FALSE(request3);
  connection = client_->CreateProtocolConnection(1);
  EXPECT_FALSE(connection);
}

TEST_F(QuicClientTest, RequestIds) {
  client_->Start();

  ConnectCallback connection_callback;
  ConnectRequest request;
  bool result = client_->Connect(quic_bridge_.kInstanceName, request,
                                 &connection_callback);
  ASSERT_TRUE(result);
  ASSERT_TRUE(request);

  quic_bridge_.RunTasksUntilIdle();
  const uint64_t instance_id = connection_callback.instance_id();
  ASSERT_TRUE(instance_id);
  std::unique_ptr<ProtocolConnection> connection =
      client_->CreateProtocolConnection(instance_id);
  ASSERT_TRUE(connection);
  EXPECT_EQ(0u, client_->GetInstanceRequestIds().GetNextRequestId(instance_id));
  EXPECT_EQ(2u, client_->GetInstanceRequestIds().GetNextRequestId(instance_id));

  connection->Close();
  quic_bridge_.RunTasksUntilIdle();
  EXPECT_EQ(4u, client_->GetInstanceRequestIds().GetNextRequestId(instance_id));

  client_->Stop();
  EXPECT_EQ(0u, client_->GetInstanceRequestIds().GetNextRequestId(instance_id));
}

}  // namespace openscreen::osp
