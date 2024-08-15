// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/statistics_collector.h"

#include <vector>

#include "cast/streaming/statistics_defines.h"
#include "gtest/gtest.h"
#include "platform/api/time.h"
#include "platform/base/span.h"
#include "platform/test/fake_clock.h"
#include "platform/test/fake_task_runner.h"
#include "util/chrono_helpers.h"

namespace openscreen::cast {

class StatisticsCollectorTest : public ::testing::Test {
 public:
  StatisticsCollectorTest()
      : fake_clock_(Clock::now()), collector_(fake_clock_.now) {}

 protected:
  FakeClock fake_clock_;
  StatisticsCollector collector_;
};

TEST_F(StatisticsCollectorTest, ReturnsEmptyIfNoEvents) {
  EXPECT_TRUE(collector_.TakeRecentPacketEvents().empty());
  EXPECT_TRUE(collector_.TakeRecentFrameEvents().empty());
}

TEST_F(StatisticsCollectorTest, CanCollectPacketEvents) {
  // clang-format off
  constexpr PacketEvent kEventOne(
      FrameId(5000),
      StatisticsEventType::kPacketSentToNetwork,
      StatisticsEventMediaType::kAudio,
      RtpTimeTicks(47474838),
      1234u,
      Clock::time_point(milliseconds(12455680)),
      Clock::time_point(milliseconds(12455685)),
      123u,
      456u);

  constexpr PacketEvent kEventTwo(
      FrameId(20000),
      StatisticsEventType::kPacketSentToNetwork,
      StatisticsEventMediaType::kVideo,
      RtpTimeTicks(4747900),
      553u,
      Clock::time_point(milliseconds(12455880)),
      Clock::time_point(milliseconds(12455889)),
      124u,
      456u);
  // clang-format on

  collector_.CollectPacketEvent(kEventOne);
  collector_.CollectPacketEvent(kEventTwo);
  const std::vector<PacketEvent> events = collector_.TakeRecentPacketEvents();

  ASSERT_EQ(2u, events.size());
  EXPECT_EQ(kEventOne, events[0]);
  EXPECT_EQ(kEventTwo, events[1]);
}

TEST_F(StatisticsCollectorTest, CanCollectPacketSentEvents) {
  constexpr std::array<uint8_t, 20> kPacketOne{
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20};
  constexpr std::array<uint8_t, 21> kPacketTwo{
      0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20};

  collector_.CollectPacketSentEvent(
      ByteView(kPacketOne),
      PacketMetadata{StreamType::kAudio, RtpTimeTicks(1234)});
  collector_.CollectPacketSentEvent(
      ByteView(kPacketTwo),
      PacketMetadata{StreamType::kVideo, RtpTimeTicks(2234)});

  const std::vector<PacketEvent> events = collector_.TakeRecentPacketEvents();
  ASSERT_EQ(2u, events.size());

  EXPECT_EQ(3856u, events[0].packet_id);
  EXPECT_EQ(4370u, events[0].max_packet_id);
  EXPECT_EQ(RtpTimeTicks(84281096), events[0].rtp_timestamp);
  EXPECT_EQ(FrameId(), events[0].frame_id);
  EXPECT_EQ(20u, events[0].size);
  EXPECT_GT(Clock::now(), events[0].timestamp);
  EXPECT_EQ(StatisticsEventType::kPacketSentToNetwork, events[0].type);
  EXPECT_EQ(StatisticsEventMediaType::kAudio, events[0].media_type);

  EXPECT_EQ(3599u, events[1].packet_id);
  EXPECT_EQ(4113u, events[1].max_packet_id);
  EXPECT_EQ(RtpTimeTicks(67438087), events[1].rtp_timestamp);
  EXPECT_EQ(FrameId(), events[1].frame_id);
  EXPECT_EQ(21u, events[1].size);
  EXPECT_GT(Clock::now(), events[1].timestamp);
  EXPECT_EQ(StatisticsEventType::kPacketSentToNetwork, events[1].type);
  EXPECT_EQ(StatisticsEventMediaType::kVideo, events[1].media_type);
}

TEST_F(StatisticsCollectorTest, CanCollectFrameEvents) {
  // clang-format off
  constexpr FrameEvent kEventOne(
      FrameId(1),
      StatisticsEventType::kFrameAckReceived,
      StatisticsEventMediaType::kVideo,
      RtpTimeTicks(1233),
      /* size= */ 0,
      Clock::time_point(milliseconds(12345678)),
      Clock::time_point(milliseconds(12345699)),
      /* width= */ 640,
      /* height= */ 480,
      /* delay_delta= */ std::chrono::milliseconds(20),
      /* key_frame= */ false,
      /* target_bitrate= */ 0);

  constexpr FrameEvent kEventTwo(
      FrameId(2),
      StatisticsEventType::kFramePlayedOut,
      StatisticsEventMediaType::kAudio,
      RtpTimeTicks(1733),
      /* size= */ 6000,
      Clock::time_point(milliseconds(12455680)),
      Clock::time_point(milliseconds(12455685)),
      /* width= */ 0,
      /* height= */ 0,
      /* delay_delta= */ std::chrono::milliseconds(10),
      /* key_frame= */ false,
      /* target_bitrate = */ 5000);
  // clang-format on

  collector_.CollectFrameEvent(kEventOne);
  collector_.CollectFrameEvent(kEventTwo);
  const std::vector<FrameEvent> events = collector_.TakeRecentFrameEvents();

  ASSERT_EQ(2u, events.size());
  EXPECT_EQ(kEventOne, events[0]);
  EXPECT_EQ(kEventTwo, events[1]);
}

}  // namespace openscreen::cast
