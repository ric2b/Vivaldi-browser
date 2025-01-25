// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp/impl/quic/quic_server.h"

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

namespace openscreen::osp {
namespace {

using ::testing::_;
using ::testing::Invoke;
using ::testing::Test;

class MockConnectRequestCallback final : public ConnectRequestCallback {
 public:
  ~MockConnectRequestCallback() override = default;

  MOCK_METHOD2(OnConnectSucceed,
               void(uint64_t request_id, uint64_t instance_id));
  MOCK_METHOD1(OnConnectFailed, void(uint64_t request_id));
};

class MockConnectionObserver final : public ProtocolConnection::Observer {
 public:
  ~MockConnectionObserver() override = default;

  MOCK_METHOD1(OnConnectionClosed, void(const ProtocolConnection& connection));
};

class QuicServerTest : public Test {
 public:
  QuicServerTest()
      : fake_clock_(Clock::time_point(std::chrono::milliseconds(1298424))),
        task_runner_(fake_clock_),
        quic_bridge_(task_runner_, FakeClock::now) {}

 protected:
  std::unique_ptr<ProtocolConnection> ExpectIncomingConnection() {
    MockConnectRequestCallback mock_connect_request_callback;
    quic_bridge_.GetQuicClient()->Connect(quic_bridge_.kInstanceName,
                                          connect_request_,
                                          &mock_connect_request_callback);
    std::unique_ptr<ProtocolConnection> stream;
    EXPECT_CALL(mock_connect_request_callback, OnConnectSucceed(_, _))
        .WillOnce(Invoke([this](uint64_t request_id, uint64_t instance_id) {
          quic_bridge_.GetQuicClient()->CreateProtocolConnection(instance_id);
        }));
    EXPECT_CALL(quic_bridge_.mock_server_observer(),
                OnIncomingConnectionMock(_))
        .WillOnce(
            Invoke([&stream](std::unique_ptr<ProtocolConnection>& connection) {
              stream = std::move(connection);
            }));
    quic_bridge_.RunTasksUntilIdle();
    return stream;
  }

  void SetUp() override {
    server_ = quic_bridge_.GetQuicServer();
    quic_bridge_.CreateNetworkServiceManager(nullptr, nullptr);
  }

  void TearDown() override { connect_request_.MarkComplete(); }

  void SendTestMessage(ProtocolConnection* connection) {
    MockMessageCallback mock_message_callback;
    MessageDemuxer::MessageWatch message_watch =
        quic_bridge_.GetControllerDemuxer().WatchMessageType(
            1, msgs::Type::kPresentationConnectionMessage,
            &mock_message_callback);

    msgs::CborEncodeBuffer buffer;
    msgs::PresentationConnectionMessage message;
    message.connection_id = 7;
    message.message.which = decltype(message.message.which)::kString;
    new (&message.message.str) std::string("message from server");
    ASSERT_TRUE(msgs::EncodePresentationConnectionMessage(message, &buffer));
    connection->Write(ByteView(buffer.data(), buffer.size()));
    connection->CloseWriteEnd();

    ssize_t decode_result = 0;
    msgs::PresentationConnectionMessage received_message;
    EXPECT_CALL(mock_message_callback,
                OnStreamMessage(
                    1, _, msgs::Type::kPresentationConnectionMessage, _, _, _))
        .WillOnce(Invoke([&decode_result, &received_message](
                             uint64_t instance_id, uint64_t connection_id,
                             msgs::Type message_type, const uint8_t* buf,
                             size_t buffer_size, Clock::time_point now) {
          decode_result = msgs::DecodePresentationConnectionMessage(
              buf, buffer_size, received_message);
          if (decode_result < 0)
            return ErrorOr<size_t>(Error::Code::kCborParsing);
          return ErrorOr<size_t>(decode_result);
        }));
    quic_bridge_.RunTasksUntilIdle();

    ASSERT_GT(decode_result, 0);
    EXPECT_EQ(decode_result, static_cast<ssize_t>(buffer.size() - 1));
    EXPECT_EQ(received_message.connection_id, message.connection_id);
    ASSERT_EQ(received_message.message.which,
              decltype(received_message.message.which)::kString);
    EXPECT_EQ(received_message.message.str, message.message.str);
  }

  ConnectRequest connect_request_;
  FakeClock fake_clock_;
  FakeTaskRunner task_runner_;
  FakeQuicBridge quic_bridge_;
  QuicServer* server_;
};

}  // namespace

TEST_F(QuicServerTest, Connect) {
  std::unique_ptr<ProtocolConnection> connection = ExpectIncomingConnection();
  ASSERT_TRUE(connection);

  SendTestMessage(connection.get());

  server_->Stop();
}

TEST_F(QuicServerTest, OpenImmediate) {
  EXPECT_FALSE(server_->CreateProtocolConnection(1));

  std::unique_ptr<ProtocolConnection> connection1 = ExpectIncomingConnection();
  ASSERT_TRUE(connection1);

  std::unique_ptr<ProtocolConnection> connection2 =
      server_->CreateProtocolConnection(connection1->GetInstanceID());

  SendTestMessage(connection2.get());

  server_->Stop();
}

TEST_F(QuicServerTest, States) {
  server_->Stop();
  EXPECT_CALL(quic_bridge_.mock_server_observer(), OnRunning());
  EXPECT_TRUE(server_->Start());
  EXPECT_FALSE(server_->Start());

  std::unique_ptr<ProtocolConnection> connection = ExpectIncomingConnection();
  ASSERT_TRUE(connection);
  MockConnectionObserver mock_connection_observer;
  connection->SetObserver(&mock_connection_observer);

  EXPECT_CALL(mock_connection_observer, OnConnectionClosed(_));
  EXPECT_CALL(quic_bridge_.mock_server_observer(), OnStopped());
  EXPECT_TRUE(server_->Stop());
  EXPECT_FALSE(server_->Stop());

  EXPECT_CALL(quic_bridge_.mock_server_observer(), OnRunning());
  EXPECT_TRUE(server_->Start());

  EXPECT_CALL(quic_bridge_.mock_server_observer(), OnSuspended());
  EXPECT_TRUE(server_->Suspend());
  EXPECT_FALSE(server_->Suspend());
  EXPECT_FALSE(server_->Start());

  EXPECT_CALL(quic_bridge_.mock_server_observer(), OnRunning());
  EXPECT_TRUE(server_->Resume());
  EXPECT_FALSE(server_->Resume());
  EXPECT_FALSE(server_->Start());

  EXPECT_CALL(quic_bridge_.mock_server_observer(), OnSuspended());
  EXPECT_TRUE(server_->Suspend());

  EXPECT_CALL(quic_bridge_.mock_server_observer(), OnStopped());
  EXPECT_TRUE(server_->Stop());
}

TEST_F(QuicServerTest, RequestIds) {
  std::unique_ptr<ProtocolConnection> connection = ExpectIncomingConnection();
  ASSERT_TRUE(connection);

  uint64_t instance_id = connection->GetInstanceID();
  EXPECT_EQ(1u, server_->GetInstanceRequestIds().GetNextRequestId(instance_id));
  EXPECT_EQ(3u, server_->GetInstanceRequestIds().GetNextRequestId(instance_id));

  connection->CloseWriteEnd();
  connection.reset();
  quic_bridge_.RunTasksUntilIdle();
  EXPECT_EQ(5u, server_->GetInstanceRequestIds().GetNextRequestId(instance_id));

  server_->Stop();
  EXPECT_EQ(1u, server_->GetInstanceRequestIds().GetNextRequestId(instance_id));
}

}  // namespace openscreen::osp
