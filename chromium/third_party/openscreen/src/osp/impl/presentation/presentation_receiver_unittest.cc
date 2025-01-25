// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp/public/presentation/presentation_receiver.h"

#include <memory>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "osp/impl/presentation/presentation_utils.h"
#include "osp/impl/presentation/testing/mock_connection_delegate.h"
#include "osp/impl/quic/quic_client.h"
#include "osp/impl/quic/quic_server.h"
#include "osp/impl/quic/testing/fake_quic_connection_factory.h"
#include "osp/impl/quic/testing/quic_test_support.h"
#include "osp/public/connect_request.h"
#include "osp/public/network_service_manager.h"
#include "osp/public/protocol_connection_server.h"
#include "osp/public/testing/message_demuxer_test_support.h"
#include "platform/base/span.h"
#include "platform/test/fake_clock.h"
#include "platform/test/fake_task_runner.h"

namespace openscreen::osp {

namespace {

using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;

class MockConnectRequestCallback final : public ConnectRequestCallback {
 public:
  ~MockConnectRequestCallback() override = default;

  MOCK_METHOD2(OnConnectSucceed,
               void(uint64_t request_id, uint64_t instance_id));
  MOCK_METHOD1(OnConnectFailed, void(uint64_t request_id));
};

class MockReceiverDelegate final : public ReceiverDelegate {
 public:
  ~MockReceiverDelegate() override = default;

  MOCK_METHOD3(
      OnUrlAvailabilityRequest,
      std::vector<msgs::UrlAvailability>(uint64_t watch_id,
                                         uint64_t watch_duration,
                                         std::vector<std::string> urls));
  MOCK_METHOD3(StartPresentation,
               bool(const Connection::PresentationInfo& info,
                    uint64_t source_id,
                    const std::vector<msgs::HttpHeader>& http_headers));
  MOCK_METHOD3(ConnectToPresentation,
               bool(uint64_t request_id,
                    const std::string& id,
                    uint64_t source_id));
  MOCK_METHOD3(TerminatePresentation,
               void(const std::string& id,
                    TerminationSource source,
                    TerminationReason reason));
};

class PresentationReceiverTest : public ::testing::Test {
 public:
  PresentationReceiverTest()
      : fake_clock_(Clock::time_point(std::chrono::milliseconds(1298424))),
        task_runner_(fake_clock_),
        quic_bridge_(task_runner_, FakeClock::now) {}

 protected:
  std::unique_ptr<ProtocolConnection> MakeClientStream() {
    MockConnectRequestCallback mock_connect_request_callback;
    quic_bridge_.GetQuicClient()->Connect(quic_bridge_.kInstanceName,
                                          connect_request_,
                                          &mock_connect_request_callback);
    EXPECT_TRUE(connect_request_);
    std::unique_ptr<ProtocolConnection> stream;
    EXPECT_CALL(mock_connect_request_callback, OnConnectSucceed(_, _))
        .WillOnce([&stream](uint64_t request_id, uint64_t instance_id) {
          stream = CreateClientProtocolConnection(instance_id);
        });
    quic_bridge_.RunTasksUntilIdle();
    return stream;
  }

  void SetUp() override {
    quic_bridge_.CreateNetworkServiceManager(nullptr, nullptr);
    ON_CALL(quic_bridge_.mock_server_observer(), OnIncomingConnectionMock(_))
        .WillByDefault(
            Invoke([this](std::unique_ptr<ProtocolConnection>& connection) {
              server_connections_.push_back(std::move(connection));
            }));
    ON_CALL(quic_bridge_.mock_client_observer(), OnIncomingConnectionMock(_))
        .WillByDefault(
            Invoke([this](std::unique_ptr<ProtocolConnection>& connection) {
              client_connections_.push_back(std::move(connection));
            }));
    receiver_.Init();
    receiver_.SetReceiverDelegate(&mock_receiver_delegate_);
  }

  void TearDown() override {
    connect_request_.MarkComplete();
    receiver_.SetReceiverDelegate(nullptr);
    receiver_.Deinit();
  }

  ConnectRequest connect_request_;
  Receiver receiver_;
  FakeClock fake_clock_;
  FakeTaskRunner task_runner_;
  const std::string url1_{"https://www.example.com/receiver.html"};
  FakeQuicBridge quic_bridge_;
  MockReceiverDelegate mock_receiver_delegate_;
  std::vector<std::unique_ptr<ProtocolConnection>> server_connections_;
  std::vector<std::unique_ptr<ProtocolConnection>> client_connections_;
};

}  // namespace

// TODO(btolsch): Availability CL includes watch duration, so when that lands,
// also test proper updating here.
TEST_F(PresentationReceiverTest, QueryAvailability) {
  MockMessageCallback mock_callback;
  MessageDemuxer::MessageWatch availability_watch =
      quic_bridge_.GetControllerDemuxer().SetDefaultMessageTypeWatch(
          msgs::Type::kPresentationUrlAvailabilityResponse, &mock_callback);

  std::unique_ptr<ProtocolConnection> stream = MakeClientStream();
  ASSERT_TRUE(stream);

  msgs::PresentationUrlAvailabilityRequest request = {
      .request_id = 0, .urls = {url1_}, .watch_duration = 0, .watch_id = 0};
  msgs::CborEncodeBuffer buffer;
  ASSERT_TRUE(msgs::EncodePresentationUrlAvailabilityRequest(request, &buffer));
  stream->Write(ByteView(buffer.data(), buffer.size()));

  EXPECT_CALL(mock_receiver_delegate_, OnUrlAvailabilityRequest(_, _, _))
      .WillOnce(Invoke([this](uint64_t watch_id, uint64_t watch_duration,
                              std::vector<std::string> urls) {
        EXPECT_EQ(std::vector<std::string>{url1_}, urls);

        return std::vector<msgs::UrlAvailability>{
            msgs::UrlAvailability::kAvailable};
      }));

  msgs::PresentationUrlAvailabilityResponse response;
  EXPECT_CALL(mock_callback, OnStreamMessage(_, _, _, _, _, _))
      .WillOnce(Invoke([&response](uint64_t instance_id, uint64_t cid,
                                   msgs::Type message_type, const uint8_t* buf,
                                   size_t buffer_size, Clock::time_point now) {
        const msgs::CborResult result =
            msgs::DecodePresentationUrlAvailabilityResponse(buf, buffer_size,
                                                            response);
        return result;
      }));
  quic_bridge_.RunTasksUntilIdle();
  EXPECT_EQ(request.request_id, response.request_id);
  EXPECT_EQ(
      (std::vector<msgs::UrlAvailability>{msgs::UrlAvailability::kAvailable}),
      response.url_availabilities);
}

TEST_F(PresentationReceiverTest, StartPresentation) {
  MockMessageCallback mock_callback;
  MessageDemuxer::MessageWatch initiation_watch =
      quic_bridge_.GetControllerDemuxer().SetDefaultMessageTypeWatch(
          msgs::Type::kPresentationStartResponse, &mock_callback);

  std::unique_ptr<ProtocolConnection> stream = MakeClientStream();
  ASSERT_TRUE(stream);

  const std::string presentation_id = "KMvyNqTCvvSv7v5X";
  msgs::PresentationStartRequest request = {
      .request_id = 0,
      .presentation_id = presentation_id,
      .url = url1_,
      .headers = {msgs::HttpHeader{"Accept-Language", "de"}}};
  msgs::CborEncodeBuffer buffer;
  ASSERT_TRUE(msgs::EncodePresentationStartRequest(request, &buffer));
  stream->Write(ByteView(buffer.data(), buffer.size()));
  Connection::PresentationInfo info;
  EXPECT_CALL(mock_receiver_delegate_, StartPresentation(_, _, request.headers))
      .WillOnce(::testing::DoAll(::testing::SaveArg<0>(&info),
                                 ::testing::Return(true)));
  quic_bridge_.RunTasksUntilIdle();
  EXPECT_EQ(presentation_id, info.id);
  EXPECT_EQ(url1_, info.url);

  NiceMock<MockConnectionDelegate> null_connection_delegate;
  Connection connection(Connection::PresentationInfo{presentation_id, url1_},
                        &null_connection_delegate, &receiver_);
  receiver_.OnPresentationStarted(presentation_id, &connection,
                                  ResponseResult::kSuccess);
  msgs::PresentationStartResponse response;
  EXPECT_CALL(mock_callback, OnStreamMessage(_, _, _, _, _, _))
      .WillOnce(Invoke([&response](uint64_t instance_id, uint64_t cid,
                                   msgs::Type message_type, const uint8_t* buf,
                                   size_t buf_size, Clock::time_point now) {
        const msgs::CborResult result =
            msgs::DecodePresentationStartResponse(buf, buf_size, response);
        return result;
      }));
  quic_bridge_.RunTasksUntilIdle();
  EXPECT_EQ(msgs::Result::kSuccess, response.result);
  EXPECT_EQ(connection.connection_id(), response.connection_id);
}

// TODO(btolsch): Connect and reconnect.
// TODO(btolsch): Terminate request and event.

}  // namespace openscreen::osp
