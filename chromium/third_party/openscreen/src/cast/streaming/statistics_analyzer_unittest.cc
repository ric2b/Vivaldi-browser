// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/statistics_analyzer.h"

#include <cstdint>
#include <numeric>

#include "cast/streaming/statistics.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "platform/base/span.h"
#include "platform/test/fake_clock.h"
#include "platform/test/fake_task_runner.h"
#include "util/chrono_helpers.h"

using testing::_;
using testing::AtLeast;
using testing::Invoke;
using testing::InvokeWithoutArgs;
using testing::Mock;
using testing::NiceMock;
using testing::Return;
using testing::Sequence;
using testing::StrictMock;

namespace openscreen::cast {

namespace {

constexpr int kDefaultStatsAnalysisIntervalMs = 500;
constexpr int kDefaultNumEvents = 20;
constexpr int kDefaultSizeBytes = 10;
constexpr int kDefaultStatIntervalMs = 5;

constexpr FrameEvent kDefaultFrameEvent(FrameId::first(),
                                        StatisticsEventType::kFrameEncoded,
                                        StatisticsEventMediaType::kVideo,
                                        RtpTimeTicks(),
                                        kDefaultSizeBytes,
                                        Clock::time_point::min(),
                                        Clock::time_point::min(),
                                        640,
                                        480,
                                        milliseconds(20),
                                        false,
                                        0);

constexpr PacketEvent kDefaultPacketEvent(
    FrameId::first(),
    StatisticsEventType::kPacketSentToNetwork,
    StatisticsEventMediaType::kVideo,
    RtpTimeTicks(),
    kDefaultSizeBytes,
    Clock::time_point::min(),
    Clock::time_point::min(),
    0u,
    100u);

void ExpectStatEq(SenderStats::StatisticsList stats_list,
                  StatisticType stat,
                  double expected_value) {
  EXPECT_DOUBLE_EQ(stats_list[static_cast<int>(stat)], expected_value)
      << GetEnumName(kStatisticTypeNames, stat).value();
}

// Checks that the first `expected_buckets.size()` entries of `recorded_buckets`
// matches the entries of `expected_buckets`. Also, checks that the total number
// of events matches for both vectors.
void ExpectHistoBuckets(const SenderStats::HistogramsList& actual_buckets_list,
                        HistogramType key,
                        const Span<const int> expected_buckets) {
  const std::vector<int>& actual_buckets =
      actual_buckets_list[static_cast<int>(key)].buckets;

  for (size_t i = 0; i < expected_buckets.size(); i++) {
    EXPECT_EQ(actual_buckets[i], expected_buckets[i])
        << GetEnumName(kHistogramTypeNames, key).value() << ", bucket=" << i;
  }

  const int total_recorded_events =
      std::accumulate(actual_buckets.begin(), actual_buckets.end(), 0);
  const int total_expected_events =
      std::accumulate(expected_buckets.begin(), expected_buckets.end(), 0);
  EXPECT_EQ(total_recorded_events, total_expected_events)
      << GetEnumName(kHistogramTypeNames, key).value();
}

class FakeSenderStatsClient : public SenderStatsClient {
 public:
  MOCK_METHOD(void, OnStatisticsUpdated, (const SenderStats&), (override));
};

class FakeClockOffsetEstimator : public ClockOffsetEstimator {
 public:
  MOCK_METHOD(void, OnFrameEvent, (const FrameEvent&), (override));
  MOCK_METHOD(void, OnPacketEvent, (const PacketEvent&), (override));
  MOCK_METHOD(std::optional<Clock::duration>,
              GetEstimatedOffset,
              (),
              (const, override));
};

}  // namespace

class StatisticsAnalyzerTest : public ::testing::Test {
 public:
  StatisticsAnalyzerTest()
      : fake_clock_(Clock::now()), fake_task_runner_(&fake_clock_) {}

  void SetUp() {
    // In general, use an estimator that doesn't have an offset.
    // TODO(issuetracker.google.com/298085631): add test coverage for the
    // estimator usage in this class.
    auto fake_estimator =
        std::make_unique<NiceMock<FakeClockOffsetEstimator>>();
    ON_CALL(*fake_estimator, GetEstimatedOffset())
        .WillByDefault(Return(Clock::duration{}));
    analyzer_ = std::make_unique<StatisticsAnalyzer>(
        &stats_client_, fake_clock_.now, fake_task_runner_,
        std::move(fake_estimator));
    collector_ = analyzer_->statistics_collector();
  }

  FrameEvent MakeFrameEvent(int frame_id, RtpTimeTicks rtp_timestamp) {
    FrameEvent event = kDefaultFrameEvent;
    event.frame_id = FrameId(frame_id);
    event.rtp_timestamp = rtp_timestamp;
    event.timestamp = fake_clock_.now();
    event.received_timestamp = event.timestamp;
    return event;
  }

  PacketEvent MakePacketEvent(int frame_and_packet_id,
                              RtpTimeTicks rtp_timestamp) {
    OSP_CHECK_LT(frame_and_packet_id, static_cast<int>(UINT16_MAX));
    PacketEvent event = kDefaultPacketEvent;
    event.packet_id = static_cast<uint16_t>(frame_and_packet_id);
    event.rtp_timestamp = rtp_timestamp;
    event.frame_id = FrameId(frame_and_packet_id);
    event.timestamp = fake_clock_.now();
    event.received_timestamp = event.timestamp;
    return event;
  }

 protected:
  NiceMock<FakeClockOffsetEstimator>* fake_estimator_;
  StrictMock<FakeSenderStatsClient> stats_client_;
  FakeClock fake_clock_;
  FakeTaskRunner fake_task_runner_;
  std::unique_ptr<StatisticsAnalyzer> analyzer_;
  StatisticsCollector* collector_ = nullptr;
};

TEST_F(StatisticsAnalyzerTest, FrameEncoded) {
  analyzer_->ScheduleAnalysis();

  Clock::time_point first_event_time = fake_clock_.now();
  Clock::time_point last_event_time;
  RtpTimeTicks rtp_timestamp;

  for (int i = 0; i < kDefaultNumEvents; i++) {
    FrameEvent event = MakeFrameEvent(i, rtp_timestamp);
    collector_->CollectFrameEvent(event);
    last_event_time = fake_clock_.now();
    fake_clock_.Advance(milliseconds(kDefaultStatIntervalMs));
    rtp_timestamp += RtpTimeDelta::FromTicks(90);
  }

  EXPECT_CALL(stats_client_, OnStatisticsUpdated(_))
      .WillOnce(Invoke([&](const SenderStats& stats) {
        double expected_fps =
            kDefaultNumEvents / (kDefaultStatsAnalysisIntervalMs / 1000.0);
        ExpectStatEq(stats.video_statistics, StatisticType::kEnqueueFps,
                     expected_fps);

        double expected_kbps =
            kDefaultSizeBytes * 8 * kDefaultNumEvents /
            static_cast<double>(kDefaultStatsAnalysisIntervalMs);
        ExpectStatEq(stats.video_statistics, StatisticType::kEncodeRateKbps,
                     expected_kbps);

        ExpectStatEq(
            stats.video_statistics, StatisticType::kFirstEventTimeMs,
            static_cast<double>(
                to_milliseconds(first_event_time.time_since_epoch()).count()));
        ExpectStatEq(
            stats.video_statistics, StatisticType::kLastEventTimeMs,
            static_cast<double>(
                to_milliseconds(last_event_time.time_since_epoch()).count()));
      }));

  fake_clock_.Advance(
      milliseconds(kDefaultStatsAnalysisIntervalMs -
                   (kDefaultStatIntervalMs * kDefaultNumEvents)));
}

TEST_F(StatisticsAnalyzerTest, FrameEncodedAndAckSent) {
  analyzer_->ScheduleAnalysis();

  Clock::duration total_frame_latency = milliseconds(0);
  RtpTimeTicks rtp_timestamp;

  for (int i = 0; i < kDefaultNumEvents; i++) {
    FrameEvent event1 = MakeFrameEvent(i, rtp_timestamp);

    // Let random frame delay be anywhere from 20 - 39 ms
    Clock::duration random_latency = milliseconds(20 + (rand() % 20));
    total_frame_latency += random_latency;

    FrameEvent event2 = MakeFrameEvent(i, rtp_timestamp);
    event2.type = StatisticsEventType::kFrameAckSent;
    event2.timestamp += random_latency;
    event2.received_timestamp += random_latency * 2;

    collector_->CollectFrameEvent(event1);
    collector_->CollectFrameEvent(event2);
    fake_clock_.Advance(milliseconds(kDefaultStatIntervalMs));
    rtp_timestamp += RtpTimeDelta::FromTicks(90);
  }

  EXPECT_CALL(stats_client_, OnStatisticsUpdated(_))
      .WillOnce(Invoke([&](const SenderStats& stats) {
        double expected_avg_frame_latency =
            static_cast<double>(to_milliseconds(total_frame_latency).count()) /
            kDefaultNumEvents;
        ExpectStatEq(stats.video_statistics, StatisticType::kAvgFrameLatencyMs,
                     expected_avg_frame_latency);
      }));

  fake_clock_.Advance(
      milliseconds(kDefaultStatsAnalysisIntervalMs -
                   (kDefaultStatIntervalMs * kDefaultNumEvents)));
}

TEST_F(StatisticsAnalyzerTest, FramePlayedOut) {
  analyzer_->ScheduleAnalysis();

  RtpTimeTicks rtp_timestamp;
  int total_late_frames = 0;

  for (int i = 0; i < kDefaultNumEvents; i++) {
    FrameEvent event1 = MakeFrameEvent(i, rtp_timestamp);

    // Let random frame delay be anywhere from 20 - 39 ms.
    Clock::duration random_latency = milliseconds(20 + (rand() % 20));

    // Frames will have delay_deltas of -20, 0, 20, 40, or 60 ms.
    auto delay_delta = milliseconds(60 - (20 * (i % 5)));

    FrameEvent event2 = MakeFrameEvent(i, rtp_timestamp);
    event2.type = StatisticsEventType::kFramePlayedOut;
    event2.timestamp += random_latency;
    event2.received_timestamp += random_latency * 2;
    event2.delay_delta = delay_delta;

    if (delay_delta > milliseconds(0)) {
      total_late_frames++;
    }

    collector_->CollectFrameEvent(event1);
    collector_->CollectFrameEvent(event2);
    fake_clock_.Advance(milliseconds(kDefaultStatIntervalMs));
    rtp_timestamp += RtpTimeDelta::FromTicks(90);
  }

  EXPECT_CALL(stats_client_, OnStatisticsUpdated(_))
      .WillOnce(Invoke([&](const SenderStats& stats) {
        ExpectStatEq(stats.video_statistics, StatisticType::kNumLateFrames,
                     total_late_frames);

        constexpr std::array<int, 6> kExpectedBuckets = {/* <0 */ 0,
                                                         /* 0-19 */ 0,
                                                         /* 20-39 */ 4,
                                                         /* 40-59 */ 4,
                                                         /* 60-79 */ 4,
                                                         /* 80-99 */ 0};
        ExpectHistoBuckets(stats.video_histograms,
                           HistogramType::kFrameLatenessMs, kExpectedBuckets);
      }));

  fake_clock_.Advance(
      milliseconds(kDefaultStatsAnalysisIntervalMs -
                   (kDefaultStatIntervalMs * kDefaultNumEvents)));
}

TEST_F(StatisticsAnalyzerTest, AllFrameEvents) {
  constexpr std::array<StatisticsEventType, 5> kEventsToReport{
      StatisticsEventType::kFrameCaptureBegin,
      StatisticsEventType::kFrameCaptureEnd, StatisticsEventType::kFrameEncoded,
      StatisticsEventType::kFrameAckSent, StatisticsEventType::kFramePlayedOut};
  constexpr int kNumFrames = 5;
  constexpr int kNumEvents = kNumFrames * kEventsToReport.size();

  constexpr int kFramePlayoutDelayDeltasMs[]{10, 14, 3, 40, 1};
  constexpr int kTimestampOffsetsMs[]{
      // clang-format off
      0, 13, 39, 278, 552,  // Frame One.
      0, 14, 34, 239,373,   // Frame Two.
      0, 19, 29, 245, 389,  // Frame Three.
      0, 17, 37, 261, 390,  // Frame Four.
      0, 14, 44, 290, 440,  // Frame Five.
      // clang-format on
  };

  analyzer_->ScheduleAnalysis();
  RtpTimeTicks rtp_timestamp;
  int current_event = 0;
  for (int frame_id = 0; frame_id < kNumFrames; frame_id++) {
    for (StatisticsEventType event_type : kEventsToReport) {
      FrameEvent event = MakeFrameEvent(frame_id, rtp_timestamp);
      event.type = event_type;
      event.timestamp += milliseconds(kTimestampOffsetsMs[current_event]);
      event.delay_delta = milliseconds(kFramePlayoutDelayDeltasMs[frame_id]);
      collector_->CollectFrameEvent(std::move(event));

      current_event++;
    }
    fake_clock_.Advance(
        milliseconds(kDefaultStatIntervalMs * kEventsToReport.size()));
    rtp_timestamp += RtpTimeDelta::FromTicks(90);
  }

  constexpr std::array<std::pair<StatisticType, double>, 7> kExpectedStats{{
      {StatisticType::kNumLateFrames, 5},
      {StatisticType::kNumFramesCaptured, 5},
      {StatisticType::kAvgEndToEndLatencyMs, 428.8},
      {StatisticType::kAvgCaptureLatencyMs, 15.4},
      {StatisticType::kAvgFrameLatencyMs, 226},
      {StatisticType::kAvgEncodeTimeMs, 21.2},
      {StatisticType::kEnqueueFps, 10},
  }};

  constexpr std::array<std::pair<HistogramType, std::array<int, 30>>, 4>
      kExpectedHistograms{{{HistogramType::kCaptureLatencyMs, {0, 5}},
                           {HistogramType::kEncodeTimeMs, {0, 1, 4}},
                           {HistogramType::kEndToEndLatencyMs,
                            {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                             0, 0, 0, 0, 1, 2, 0, 0, 1, 0, 0, 0, 0, 1

                            }},
                           {HistogramType::kFrameLatenessMs, {0, 4, 0, 1}}}};

  EXPECT_CALL(stats_client_, OnStatisticsUpdated(_))
      .WillOnce(Invoke([&](const SenderStats& stats) {
        for (const auto& stat_pair : kExpectedStats) {
          ExpectStatEq(stats.video_statistics, stat_pair.first,
                       stat_pair.second);
        }
        for (const auto& histogram_pair : kExpectedHistograms) {
          ExpectHistoBuckets(stats.video_histograms, histogram_pair.first,
                             histogram_pair.second);
        }
      }));

  fake_clock_.Advance(milliseconds(kDefaultStatsAnalysisIntervalMs -
                                   (kDefaultStatIntervalMs * kNumEvents)));
}

TEST_F(StatisticsAnalyzerTest, FrameEncodedAndPacketSent) {
  analyzer_->ScheduleAnalysis();

  Clock::duration total_queueing_latency = milliseconds(0);
  RtpTimeTicks rtp_timestamp;

  for (int i = 0; i < kDefaultNumEvents; i++) {
    FrameEvent event1 = MakeFrameEvent(i, rtp_timestamp);

    // Let queueing latency be either 0, 20, 40, 60, or 80 ms.
    const Clock::duration queueing_latency = milliseconds(80 - (20 * (i % 5)));
    total_queueing_latency += queueing_latency;

    PacketEvent event2 = MakePacketEvent(i, rtp_timestamp);
    event2.timestamp += queueing_latency;

    collector_->CollectFrameEvent(event1);
    collector_->CollectPacketEvent(event2);
    fake_clock_.Advance(milliseconds(kDefaultStatIntervalMs));
    rtp_timestamp += RtpTimeDelta::FromTicks(90);
  }

  EXPECT_CALL(stats_client_, OnStatisticsUpdated(_))
      .WillOnce(Invoke([&](const SenderStats& stats) {
        constexpr double kExpectedKbps =
            kDefaultSizeBytes * 8 * kDefaultNumEvents /
            static_cast<double>(kDefaultStatsAnalysisIntervalMs);
        ExpectStatEq(stats.video_statistics,
                     StatisticType::kPacketTransmissionRateKbps, kExpectedKbps);

        const double expected_avg_queueing_latency =
            static_cast<double>(
                to_milliseconds(total_queueing_latency).count()) /
            kDefaultNumEvents;
        ExpectStatEq(stats.video_statistics,
                     StatisticType::kAvgQueueingLatencyMs,
                     expected_avg_queueing_latency);

        constexpr std::array<int, 7> kExpectedBuckets = {/* <0 */ 0,
                                                         /* 0-19 */ 4,
                                                         /* 20-39 */ 4,
                                                         /* 40-59 */ 4,
                                                         /* 60-79 */ 4,
                                                         /* 80-99 */ 4,
                                                         /* 100-119 */ 0};
        ExpectHistoBuckets(stats.video_histograms,
                           HistogramType::kQueueingLatencyMs, kExpectedBuckets);
      }));

  fake_clock_.Advance(
      milliseconds(kDefaultStatsAnalysisIntervalMs -
                   (kDefaultStatIntervalMs * kDefaultNumEvents)));
}

TEST_F(StatisticsAnalyzerTest, PacketSentAndReceived) {
  analyzer_->ScheduleAnalysis();

  Clock::duration total_network_latency = milliseconds(0);
  RtpTimeTicks rtp_timestamp;

  for (int i = 0; i < kDefaultNumEvents; i++) {
    PacketEvent event1 = MakePacketEvent(i, rtp_timestamp);

    // Let network latency be either 0, 20, 40, 60, or 80 ms.
    Clock::duration network_latency = milliseconds(80 - (20 * (i % 5)));
    total_network_latency += network_latency;

    PacketEvent event2 = MakePacketEvent(i, rtp_timestamp);
    event2.frame_id = FrameId(i);
    event2.timestamp += network_latency;
    event2.received_timestamp += network_latency * 2;
    event2.type = StatisticsEventType::kPacketReceived;

    collector_->CollectPacketEvent(event1);
    collector_->CollectPacketEvent(event2);
    fake_clock_.Advance(milliseconds(kDefaultStatIntervalMs));
    rtp_timestamp += RtpTimeDelta::FromTicks(90);
  }

  EXPECT_CALL(stats_client_, OnStatisticsUpdated(_))
      .WillOnce(Invoke([&](const SenderStats& stats) {
        const double expected_avg_network_latency =
            static_cast<double>(
                to_milliseconds(total_network_latency).count()) /
            kDefaultNumEvents;
        ExpectStatEq(stats.video_statistics,
                     StatisticType::kAvgNetworkLatencyMs,
                     expected_avg_network_latency);

        constexpr std::array<int, 7> kExpectedBuckets = {/* <0 */ 0,
                                                         /* 0-19 */ 4,
                                                         /* 20-39 */ 4,
                                                         /* 40-59 */ 4,
                                                         /* 60-79 */ 4,
                                                         /* 80-99 */ 4,
                                                         /* 100-119 */ 0};
        ExpectHistoBuckets(stats.video_histograms,
                           HistogramType::kNetworkLatencyMs, kExpectedBuckets);
      }));

  fake_clock_.Advance(
      milliseconds(kDefaultStatsAnalysisIntervalMs -
                   (kDefaultStatIntervalMs * kDefaultNumEvents)));
}

TEST_F(StatisticsAnalyzerTest, FrameEncodedPacketSentAndReceived) {
  analyzer_->ScheduleAnalysis();

  Clock::duration total_packet_latency = milliseconds(0);
  RtpTimeTicks rtp_timestamp;
  Clock::time_point last_event_time;

  for (int i = 0; i < kDefaultNumEvents; i++) {
    FrameEvent event1 = MakeFrameEvent(i, rtp_timestamp);

    PacketEvent event2 = MakePacketEvent(i, rtp_timestamp);

    // Let packet latency be either 20, 40, 60, 80, or 100 ms.
    Clock::duration packet_latency = milliseconds(100 - (20 * (i % 5)));
    total_packet_latency += packet_latency;
    if (fake_clock_.now() + packet_latency > last_event_time) {
      last_event_time = fake_clock_.now() + packet_latency;
    }

    PacketEvent event3 = MakePacketEvent(i, rtp_timestamp);
    event3.timestamp += packet_latency;
    event3.received_timestamp += packet_latency * 2;
    event3.type = StatisticsEventType::kPacketReceived;

    collector_->CollectFrameEvent(event1);
    collector_->CollectPacketEvent(event2);
    collector_->CollectPacketEvent(event3);
    fake_clock_.Advance(milliseconds(kDefaultStatIntervalMs));
    rtp_timestamp += RtpTimeDelta::FromTicks(90);
  }

  EXPECT_CALL(stats_client_, OnStatisticsUpdated(_))
      .WillOnce(Invoke([&](const SenderStats& stats) {
        ExpectStatEq(stats.video_statistics, StatisticType::kNumPacketsSent,
                     kDefaultNumEvents);
        ExpectStatEq(stats.video_statistics, StatisticType::kNumPacketsReceived,
                     kDefaultNumEvents);

        const double expected_time_since_last_receiver_response =
            static_cast<double>(
                (to_milliseconds(fake_clock_.now() - last_event_time) -
                 milliseconds(25))
                    .count());
        ExpectStatEq(stats.video_statistics,
                     StatisticType::kTimeSinceLastReceiverResponseMs,
                     expected_time_since_last_receiver_response);

        const double expected_avg_packet_latency =
            static_cast<double>(to_milliseconds(total_packet_latency).count()) /
            kDefaultNumEvents;
        ExpectStatEq(stats.video_statistics, StatisticType::kAvgPacketLatencyMs,
                     expected_avg_packet_latency);

        constexpr std::array<int, 8> kExpectedBuckets = {/* <0 */ 0,
                                                         /* 0-19 */ 0,
                                                         /* 20-39 */ 4,
                                                         /* 40-59 */ 4,
                                                         /* 60-79 */ 4,
                                                         /* 80-99 */ 4,
                                                         /* 100-119 */ 4,
                                                         /* 120-139 */ 0};
        ExpectHistoBuckets(stats.video_histograms,
                           HistogramType::kPacketLatencyMs, kExpectedBuckets);
      }));

  fake_clock_.Advance(
      milliseconds(kDefaultStatsAnalysisIntervalMs -
                   (kDefaultStatIntervalMs * kDefaultNumEvents)));
}

TEST_F(StatisticsAnalyzerTest, AudioAndVideoFrameEncodedPacketSentAndReceived) {
  analyzer_->ScheduleAnalysis();

  const int num_events = 100;
  const int frame_interval_ms = 2;

  RtpTimeTicks rtp_timestamp;
  Clock::duration total_audio_packet_latency = milliseconds(0);
  Clock::duration total_video_packet_latency = milliseconds(0);
  int total_audio_events = 0;
  int total_video_events = 0;

  for (int i = 0; i < num_events; i++) {
    StatisticsEventMediaType media_type = StatisticsEventMediaType::kVideo;
    if (i % 2 == 0) {
      media_type = StatisticsEventMediaType::kAudio;
    }

    FrameEvent event1 = MakeFrameEvent(i, rtp_timestamp);
    event1.media_type = media_type;

    PacketEvent event2 = MakePacketEvent(i, rtp_timestamp);
    event2.timestamp += milliseconds(5);
    event2.media_type = media_type;

    // Let packet latency be either 20, 40, 60, 80, or 100 ms.
    Clock::duration packet_latency = milliseconds(100 - (20 * (i % 5)));
    if (media_type == StatisticsEventMediaType::kAudio) {
      total_audio_events++;
      total_audio_packet_latency += packet_latency;
    } else if (media_type == StatisticsEventMediaType::kVideo) {
      total_video_events++;
      total_video_packet_latency += packet_latency;
    }

    PacketEvent event3 = MakePacketEvent(i, rtp_timestamp);
    event3.timestamp += packet_latency;
    event3.type = StatisticsEventType::kPacketReceived;
    event3.media_type = media_type;

    collector_->CollectFrameEvent(event1);
    collector_->CollectPacketEvent(event2);
    collector_->CollectPacketEvent(event3);
    fake_clock_.Advance(milliseconds(frame_interval_ms));
    rtp_timestamp += RtpTimeDelta::FromTicks(90);
  }

  EXPECT_CALL(stats_client_, OnStatisticsUpdated(_))
      .WillOnce(Invoke([&](const SenderStats& stats) {
        ExpectStatEq(stats.audio_statistics, StatisticType::kNumPacketsSent,
                     total_audio_events);
        ExpectStatEq(stats.audio_statistics, StatisticType::kNumPacketsReceived,
                     total_audio_events);
        ExpectStatEq(stats.video_statistics, StatisticType::kNumPacketsSent,
                     total_video_events);
        ExpectStatEq(stats.video_statistics, StatisticType::kNumPacketsReceived,
                     total_video_events);

        double expected_audio_avg_packet_latency =
            static_cast<double>(
                to_milliseconds(total_audio_packet_latency).count()) /
            total_audio_events;
        ExpectStatEq(stats.audio_statistics, StatisticType::kAvgPacketLatencyMs,
                     expected_audio_avg_packet_latency);
        double expected_video_avg_packet_latency =
            static_cast<double>(
                to_milliseconds(total_video_packet_latency).count()) /
            total_video_events;
        ExpectStatEq(stats.video_statistics, StatisticType::kAvgPacketLatencyMs,
                     expected_video_avg_packet_latency);
      }));

  fake_clock_.Advance(milliseconds(kDefaultStatsAnalysisIntervalMs -
                                   (frame_interval_ms * num_events)));
}

TEST_F(StatisticsAnalyzerTest, LotsOfEventsStillWorksProperly) {
  constexpr std::array<StatisticsEventType, 5> kEventsToReport{
      StatisticsEventType::kFrameCaptureBegin,
      StatisticsEventType::kFrameCaptureEnd, StatisticsEventType::kFrameEncoded,
      StatisticsEventType::kFrameAckSent, StatisticsEventType::kFramePlayedOut};
  constexpr int kNumFrames = 1000;
  constexpr int kNumEvents = kNumFrames * kEventsToReport.size();

  constexpr std::array<int, 5> kFramePlayoutDelayDeltasMs{10, 14, 3, 40, 1};
  constexpr std::array<int, 25> kTimestampOffsetsMs{
      // clang-format off
      0, 13, 39, 278, 552,   // Frame One.
      0, 14, 34, 239, 373,   // Frame Two.
      0, 19, 29, 245, 389,   // Frame Three.
      0, 17, 37, 261, 390,   // Frame Four.
      0, 14, 44, 290, 440,   // Frame Five.
      // clang-format on
  };

  constexpr std::array<std::pair<StatisticType, double>, 7> kExpectedStats{{
      {StatisticType::kNumLateFrames, 1000},
      {StatisticType::kNumFramesCaptured, 1000},
      {StatisticType::kAvgEndToEndLatencyMs, 428.8},
      {StatisticType::kAvgCaptureLatencyMs, 15.4},
      {StatisticType::kAvgFrameLatencyMs, 226},
      {StatisticType::kAvgEncodeTimeMs, 21.2},
      {StatisticType::kEnqueueFps, 40},
  }};

  constexpr std::array<std::pair<HistogramType, std::array<int, 30>>, 4>
      kExpectedHistograms{
          {{HistogramType::kCaptureLatencyMs, {0, 1000}},
           {HistogramType::kEncodeTimeMs, {0, 200, 800}},
           {HistogramType::kEndToEndLatencyMs,
            {0, 0, 0, 0, 0,   0,   0, 0, 0,   0, 0, 0, 0, 0,  0,
             0, 0, 0, 0, 200, 400, 0, 0, 200, 0, 0, 0, 0, 200

            }},
           {HistogramType::kFrameLatenessMs, {0, 800, 0, 200}}}};

  // We don't check stats the first 49 times, only the last.
  {
    testing::InSequence s;
    EXPECT_CALL(stats_client_, OnStatisticsUpdated(_)).Times(49);
    EXPECT_CALL(stats_client_, OnStatisticsUpdated(_))
        .WillOnce(Invoke([&](const SenderStats& stats) {
          for (const auto& stat_pair : kExpectedStats) {
            ExpectStatEq(stats.video_statistics, stat_pair.first,
                         stat_pair.second);
          }
          for (const auto& histogram_pair : kExpectedHistograms) {
            ExpectHistoBuckets(stats.video_histograms, histogram_pair.first,
                               histogram_pair.second);
          }
        }));
  }

  analyzer_->ScheduleAnalysis();
  RtpTimeTicks rtp_timestamp;
  int current_event = 0;
  for (int frame_id = 0; frame_id < kNumFrames; frame_id++) {
    for (StatisticsEventType event_type : kEventsToReport) {
      FrameEvent event = MakeFrameEvent(frame_id, rtp_timestamp);
      event.type = event_type;
      event.timestamp += milliseconds(
          kTimestampOffsetsMs[current_event % kTimestampOffsetsMs.size()]);
      event.delay_delta = milliseconds(
          kFramePlayoutDelayDeltasMs[frame_id %
                                     kFramePlayoutDelayDeltasMs.size()]);
      collector_->CollectFrameEvent(std::move(event));

      current_event++;
    }
    fake_clock_.Advance(
        milliseconds(kDefaultStatIntervalMs * kEventsToReport.size()));
    rtp_timestamp += RtpTimeDelta::FromTicks(90);
  }

  fake_clock_.Advance(milliseconds(kDefaultStatsAnalysisIntervalMs -
                                   (kDefaultStatIntervalMs * kNumEvents)));
}

}  // namespace openscreen::cast
