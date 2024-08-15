// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STREAMING_STATISTICS_ANALYZER_H_
#define CAST_STREAMING_STATISTICS_ANALYZER_H_

#include <map>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "cast/streaming/clock_offset_estimator.h"
#include "cast/streaming/statistics.h"
#include "cast/streaming/statistics_collector.h"
#include "platform/api/time.h"
#include "util/alarm.h"

namespace openscreen::cast {

class StatisticsAnalyzer {
 public:
  StatisticsAnalyzer(SenderStatsClient* stats_client,
                     ClockNowFunctionPtr now,
                     TaskRunner& task_runner,
                     std::unique_ptr<ClockOffsetEstimator> offset_estimator);
  ~StatisticsAnalyzer();

  void ScheduleAnalysis();

  // Get the statistics collector managed by this analyzer.
  StatisticsCollector* statistics_collector() {
    return statistics_collector_.get();
  }

 private:
  struct FrameStatsAggregate {
    int event_counter;
    uint32_t sum_size;
    Clock::duration sum_delay;
  };

  struct PacketStatsAggregate {
    int event_counter;
    uint32_t sum_size;
  };

  struct LatencyStatsAggregate {
    int data_point_counter;
    Clock::duration sum_latency;
  };

  struct FrameInfo {
    Clock::time_point capture_begin_time = Clock::time_point::min();
    Clock::time_point capture_end_time = Clock::time_point::min();
    Clock::time_point encode_end_time = Clock::time_point::min();
  };

  struct PacketInfo {
    Clock::time_point timestamp;
    StatisticsEventType type;
  };

  struct SessionStats {
    Clock::time_point first_event_time = Clock::time_point::max();
    Clock::time_point last_event_time = Clock::time_point::min();
    Clock::time_point last_response_received_time = Clock::time_point::min();
    int late_frame_counter = 0;
  };

  // Named std::pair equivalent for audio + video classes.
  template <typename T>
  struct AVPair {
    T audio;
    T video;

    const T& Get(StatisticsEventMediaType media_type) const {
      if (media_type == StatisticsEventMediaType::kAudio) {
        return audio;
      }
      OSP_CHECK(media_type == StatisticsEventMediaType::kVideo);
      return video;
    }
    T& Get(StatisticsEventMediaType media_type) {
      return const_cast<T&>(const_cast<const AVPair*>(this)->Get(media_type));
    }
  };

  using FrameStatsMap = std::map<StatisticsEventType, FrameStatsAggregate>;
  using PacketStatsMap = std::map<StatisticsEventType, PacketStatsAggregate>;
  using LatencyStatsMap = std::map<StatisticType, LatencyStatsAggregate>;

  using FrameInfoMap = std::map<RtpTimeTicks, FrameInfo>;
  using PacketKey = std::pair<RtpTimeTicks, uint16_t>;
  using PacketInfoMap = std::map<PacketKey, PacketInfo>;

  // Initialize the stats histograms with the preferred min, max, and width.
  void InitHistograms();

  // Takes the Frame and Packet events from the `collector_`, and processes them
  // into a form expected by `stats_client_`. Then sends the stats, and
  // schedules a future analysis.
  void AnalyzeStatistics();

  // Constructs a stats list, and sends it to `stats_client_`;
  void SendStatistics();

  // Handles incoming stat events, and adds their infos to all of the proper
  // stats maps / aggregates.
  void ProcessFrameEvents(const std::vector<FrameEvent>& frame_events);
  void ProcessPacketEvents(const std::vector<PacketEvent>& packet_events);
  void RecordFrameLatencies(const FrameEvent& frame_event);
  void RecordPacketLatencies(const PacketEvent& packet_event);
  void RecordEventTimes(const StatisticsEvent& event);
  void ErasePacketInfo(const PacketEvent& packet_event);
  void AddToLatencyAggregrate(StatisticType latency_stat,
                              Clock::duration latency_delta,
                              StatisticsEventMediaType media_type);
  void AddToHistogram(HistogramType histogram,
                      StatisticsEventMediaType media_type,
                      int64_t sample);

  // Creates a stats list, and populates the entries based on stored stats info
  // / aggregates for each stat field.
  SenderStats::StatisticsList ConstructStatisticsList(
      Clock::time_point end_time,
      StatisticsEventMediaType media_type);

  void PopulatePacketCountStat(StatisticsEventType event,
                               StatisticType stat,
                               StatisticsEventMediaType media_type,
                               SenderStats::StatisticsList& stats_list);

  void PopulateFrameCountStat(StatisticsEventType event,
                              StatisticType stat,
                              StatisticsEventMediaType media_type,
                              SenderStats::StatisticsList& stats_list);

  void PopulateFpsStat(StatisticsEventType event,
                       StatisticType stat,
                       StatisticsEventMediaType media_type,
                       Clock::time_point end_time,
                       SenderStats::StatisticsList& stats_list);

  void PopulateAvgLatencyStat(StatisticType stat,
                              StatisticsEventMediaType media_type,
                              SenderStats::StatisticsList& stats_list);

  void PopulateFrameBitrateStat(StatisticsEventType event,
                                StatisticType stat,
                                StatisticsEventMediaType media_type,
                                Clock::time_point end_time,
                                SenderStats::StatisticsList& stats_list);

  void PopulatePacketBitrateStat(StatisticsEventType event,
                                 StatisticType stat,
                                 StatisticsEventMediaType media_type,
                                 Clock::time_point end_time,
                                 SenderStats::StatisticsList& stats_list);

  void PopulateSessionStats(StatisticsEventMediaType media_type,
                            Clock::time_point end_time,
                            SenderStats::StatisticsList& stats_list);

  // Calculates the offset between the sender and receiver clocks and returns
  // the sender-side version of this receiver timestamp, if possible.
  std::optional<Clock::time_point> ToSenderTimestamp(
      Clock::time_point receiver_timestamp,
      StatisticsEventMediaType media_type) const;

  // Records the network latency estimate, which is then weighted and used as
  // part of the moving network latency estimate.
  void RecordEstimatedNetworkLatency(Clock::duration latency);

  // The statistics client to which we report analyzed statistics.
  SenderStatsClient* const stats_client_;

  // The statistics collector from which we take the un-analyzed stats packets.
  std::unique_ptr<StatisticsCollector> statistics_collector_;

  // Keeps track of the best-guess clock offset between the sender and receiver.
  std::unique_ptr<ClockOffsetEstimator> offset_estimator_;

  // Keep track of time and events for this analyzer.
  ClockNowFunctionPtr now_;
  Alarm alarm_;
  Clock::time_point start_time_;

  // Keep track of the currently estimated network latency.
  //
  // NOTE: though we currently record the average network latency separately for
  // audio and video, they use the same network so the value should be the same.
  Clock::duration estimated_network_latency_{};

  // Maps of frame / packet infos used for stats that rely on seeing multiple
  // events. For example, network latency is the calculated time difference
  // between went a packet is sent, and when it is received.
  AVPair<FrameInfoMap> recent_frame_infos_;
  AVPair<PacketInfoMap> recent_packet_infos_;

  // Aggregate statistics.
  AVPair<FrameStatsMap> frame_stats_;
  AVPair<PacketStatsMap> packet_stats_;
  AVPair<LatencyStatsMap> latency_stats_;

  // Stats that relate to the entirety of the session. For example, total late
  // frames, or time of last event.
  AVPair<SessionStats> session_stats_;

  // Histograms.
  AVPair<SenderStats::HistogramsList> histograms_;
};

}  // namespace openscreen::cast

#endif  // CAST_STREAMING_STATISTICS_ANALYZER_H_
