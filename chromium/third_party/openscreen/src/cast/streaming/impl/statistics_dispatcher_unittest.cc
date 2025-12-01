// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/impl/statistics_dispatcher.h"

#include "cast/streaming/encoded_frame.h"
#include "cast/streaming/impl/rtcp_common.h"
#include "cast/streaming/impl/rtp_defines.h"
#include "cast/streaming/impl/statistics_collector.h"
#include "cast/streaming/impl/statistics_defines.h"
#include "cast/streaming/public/frame_id.h"
#include "cast/streaming/testing/mock_environment.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "platform/api/time.h"
#include "platform/base/error.h"
#include "platform/test/fake_clock.h"
#include "platform/test/fake_task_runner.h"
#include "util/chrono_helpers.h"

namespace openscreen::cast {

namespace {

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Mock;
using ::testing::SaveArg;
using ::testing::StrictMock;

class StatisticsDispatcherTest : public ::testing::Test {
 public:
  StatisticsDispatcherTest()
      : environment_(&FakeClock::now, task_runner_),
        collector_(&clock_.now),
        dispatcher_(environment_) {
    environment_.SetStatisticsCollector(&collector_);
  }

  ~StatisticsDispatcherTest() override {
    environment_.SetStatisticsCollector(nullptr);
  }

 protected:
  FakeClock clock_{Clock::now()};
  FakeTaskRunner task_runner_{clock_};
  testing::NiceMock<MockEnvironment> environment_;
  StatisticsCollector collector_;
  StatisticsDispatcher dispatcher_;
};

TEST_F(StatisticsDispatcherTest, DispatchEnqueueEvents) {
  EncodedFrame frame;
  frame.rtp_timestamp = RtpTimeTicks(12345);
  frame.frame_id = FrameId::first();
  frame.dependency = EncodedFrame::Dependency::kKeyFrame;
  frame.data = ByteView(reinterpret_cast<const uint8_t*>("test"), 4);
  frame.capture_begin_time = clock_.now() + milliseconds(10);
  frame.capture_end_time = clock_.now() + milliseconds(20);

  dispatcher_.DispatchEnqueueEvents(StreamType::kVideo, frame);
  const std::vector<FrameEvent> events = collector_.TakeRecentFrameEvents();
  ASSERT_EQ(3u, events.size());
  EXPECT_EQ(events[0].type, StatisticsEventType::kFrameCaptureBegin);
  EXPECT_EQ(events[0].media_type, StatisticsEventMediaType::kVideo);
  EXPECT_EQ(events[0].rtp_timestamp, frame.rtp_timestamp);
  EXPECT_EQ(events[0].timestamp, frame.capture_begin_time);

  EXPECT_EQ(events[1].type, StatisticsEventType::kFrameCaptureEnd);
  EXPECT_EQ(events[1].media_type, StatisticsEventMediaType::kVideo);
  EXPECT_EQ(events[1].rtp_timestamp, frame.rtp_timestamp);
  EXPECT_EQ(events[1].timestamp, frame.capture_end_time);

  EXPECT_EQ(events[2].type, StatisticsEventType::kFrameEncoded);
  EXPECT_EQ(events[2].media_type, StatisticsEventMediaType::kVideo);
  EXPECT_EQ(events[2].rtp_timestamp, frame.rtp_timestamp);
  EXPECT_EQ(events[2].frame_id, frame.frame_id);
  EXPECT_EQ(events[2].size, 4u);
  EXPECT_EQ(events[2].key_frame, true);
}

TEST_F(StatisticsDispatcherTest, DispatchEnqueueEventsWithDefaultTimes) {
  EncodedFrame frame;
  frame.rtp_timestamp = RtpTimeTicks(12345);
  frame.frame_id = FrameId::first();
  frame.dependency = EncodedFrame::Dependency::kKeyFrame;
  frame.data = ByteView(reinterpret_cast<const uint8_t*>("test"), 4);

  dispatcher_.DispatchEnqueueEvents(StreamType::kVideo, frame);
  const std::vector<FrameEvent> events = collector_.TakeRecentFrameEvents();
  ASSERT_EQ(3u, events.size());

  EXPECT_EQ(events[0].type, StatisticsEventType::kFrameCaptureBegin);
  EXPECT_EQ(events[0].media_type, StatisticsEventMediaType::kVideo);
  EXPECT_EQ(events[0].rtp_timestamp, frame.rtp_timestamp);
  EXPECT_EQ(events[0].timestamp, clock_.now());

  EXPECT_EQ(events[1].type, StatisticsEventType::kFrameCaptureEnd);
  EXPECT_EQ(events[1].media_type, StatisticsEventMediaType::kVideo);
  EXPECT_EQ(events[1].rtp_timestamp, frame.rtp_timestamp);
  EXPECT_EQ(events[1].timestamp, clock_.now());

  EXPECT_EQ(events[2].type, StatisticsEventType::kFrameEncoded);
  EXPECT_EQ(events[2].media_type, StatisticsEventMediaType::kVideo);
  EXPECT_EQ(events[2].rtp_timestamp, frame.rtp_timestamp);
  EXPECT_EQ(events[2].frame_id, frame.frame_id);
  EXPECT_EQ(events[2].size, 4u);
  EXPECT_EQ(events[2].key_frame, true);
}

TEST_F(StatisticsDispatcherTest, DispatchAckEvent) {
  const RtpTimeTicks kRtpTimestamp(54321);
  const FrameId kFrameId = FrameId::first() + 1;

  dispatcher_.DispatchAckEvent(StreamType::kAudio, kRtpTimestamp, kFrameId);
  const std::vector<FrameEvent> events = collector_.TakeRecentFrameEvents();

  EXPECT_EQ(events[0].type, StatisticsEventType::kFrameAckReceived);
  EXPECT_EQ(events[0].media_type, StatisticsEventMediaType::kAudio);
  EXPECT_EQ(events[0].rtp_timestamp, kRtpTimestamp);
  EXPECT_EQ(events[0].frame_id, kFrameId);
}

TEST_F(StatisticsDispatcherTest, DispatchFrameLogMessages) {
  std::vector<RtcpReceiverFrameLogMessage> messages;
  RtcpReceiverFrameLogMessage log_message;
  log_message.rtp_timestamp = RtpTimeTicks(98765);

  RtcpReceiverEventLogMessage packet_received_message;
  packet_received_message.type = StatisticsEventType::kPacketReceived;
  packet_received_message.timestamp = clock_.now() + milliseconds(5);
  packet_received_message.packet_id = 10;
  log_message.messages.push_back(packet_received_message);

  RtcpReceiverEventLogMessage frame_ack_sent_message;
  frame_ack_sent_message.type = StatisticsEventType::kFrameAckSent;
  frame_ack_sent_message.timestamp = clock_.now() + milliseconds(10);
  log_message.messages.push_back(frame_ack_sent_message);

  RtcpReceiverEventLogMessage frame_decoded_message;
  frame_decoded_message.type = StatisticsEventType::kFrameDecoded;
  frame_decoded_message.timestamp = clock_.now() + milliseconds(15);
  log_message.messages.push_back(frame_decoded_message);

  RtcpReceiverEventLogMessage frame_played_out_message;
  frame_played_out_message.type = StatisticsEventType::kFramePlayedOut;
  frame_played_out_message.timestamp = clock_.now() + milliseconds(20);
  frame_played_out_message.delay = milliseconds(10);
  log_message.messages.push_back(frame_played_out_message);
  messages.push_back(log_message);

  dispatcher_.DispatchFrameLogMessages(StreamType::kAudio, messages);
  const std::vector<FrameEvent> frame_events =
      collector_.TakeRecentFrameEvents();
  const std::vector<PacketEvent> packet_events =
      collector_.TakeRecentPacketEvents();
  ASSERT_EQ(3u, frame_events.size());
  ASSERT_EQ(1u, packet_events.size());

  EXPECT_EQ(packet_events[0].type, StatisticsEventType::kPacketReceived);
  EXPECT_EQ(packet_events[0].media_type, StatisticsEventMediaType::kAudio);
  EXPECT_EQ(packet_events[0].rtp_timestamp, log_message.rtp_timestamp);
  EXPECT_EQ(packet_events[0].packet_id, packet_received_message.packet_id);
  EXPECT_EQ(packet_events[0].timestamp, packet_received_message.timestamp);
  EXPECT_EQ(packet_events[0].received_timestamp, clock_.now());

  EXPECT_EQ(frame_events[0].type, StatisticsEventType::kFrameAckSent);
  EXPECT_EQ(frame_events[0].media_type, StatisticsEventMediaType::kAudio);
  EXPECT_EQ(frame_events[0].rtp_timestamp, log_message.rtp_timestamp);
  EXPECT_EQ(frame_events[0].timestamp, frame_ack_sent_message.timestamp);
  EXPECT_EQ(frame_events[0].received_timestamp, clock_.now());

  EXPECT_EQ(frame_events[1].type, StatisticsEventType::kFrameDecoded);
  EXPECT_EQ(frame_events[1].media_type, StatisticsEventMediaType::kAudio);
  EXPECT_EQ(frame_events[1].rtp_timestamp, log_message.rtp_timestamp);
  EXPECT_EQ(frame_events[1].timestamp, frame_decoded_message.timestamp);
  EXPECT_EQ(frame_events[1].received_timestamp, clock_.now());

  EXPECT_EQ(frame_events[2].type, StatisticsEventType::kFramePlayedOut);
  EXPECT_EQ(frame_events[2].media_type, StatisticsEventMediaType::kAudio);
  EXPECT_EQ(frame_events[2].rtp_timestamp, log_message.rtp_timestamp);
  EXPECT_EQ(frame_events[2].timestamp, frame_played_out_message.timestamp);
  EXPECT_EQ(frame_events[2].received_timestamp, clock_.now());
  EXPECT_EQ(frame_events[2].delay_delta, frame_played_out_message.delay);
}

TEST_F(StatisticsDispatcherTest, DispatchFrameLogMessagesWithUnknownEventType) {
  std::vector<RtcpReceiverFrameLogMessage> messages;
  RtcpReceiverFrameLogMessage log_message;
  log_message.rtp_timestamp = RtpTimeTicks(98765);

  RtcpReceiverEventLogMessage unknown_event_message;
  unknown_event_message.type = StatisticsEventType::kUnknown;
  unknown_event_message.timestamp = clock_.now() + milliseconds(5);
  log_message.messages.push_back(unknown_event_message);

  messages.push_back(log_message);

  dispatcher_.DispatchFrameLogMessages(StreamType::kAudio, messages);

  const std::vector<FrameEvent> frame_events =
      collector_.TakeRecentFrameEvents();
  const std::vector<PacketEvent> packet_events =
      collector_.TakeRecentPacketEvents();
  EXPECT_EQ(0u, frame_events.size());
  EXPECT_EQ(0u, packet_events.size());
}

}  // namespace
}  // namespace openscreen::cast
