// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/public/statistics.h"

#include <algorithm>
#include <iomanip>
#include <iostream>

#include "absl/strings/str_join.h"
#include "util/enum_name_table.h"
#include "util/json/json_helpers.h"
#include "util/json/json_serialization.h"
#include "util/stringprintf.h"

namespace openscreen::cast {

namespace {

template <typename Type>
Json::Value ToJson(const Type& t) {
  return t.ToJson();
}

template <>
Json::Value ToJson(const double& t) {
  return t;
}

template <typename T, typename Type>
Json::Value ArrayToJson(
    const std::array<T, static_cast<size_t>(Type::kNumTypes)>& list,
    const EnumNameTable<Type, static_cast<size_t>(Type::kNumTypes)>& names) {
  Json::Value out;
  for (size_t i = 0; i < list.size(); ++i) {
    ErrorOr<const char*> name = GetEnumName(names, static_cast<Type>(i));
    OSP_CHECK(name);
    out[name.value()] = ToJson(list[i]);
  }
  return out;
}

}  // namespace

// External linkage for unit test
extern const EnumNameTable<StatisticType,
                           static_cast<size_t>(StatisticType::kNumTypes)>
    kStatisticTypeNames = {
        {{"EnqueueFps", StatisticType::kEnqueueFps},
         {"AvgCaptureLatencyMs", StatisticType::kAvgCaptureLatencyMs},
         {"AvgEncodeTimeMs", StatisticType::kAvgEncodeTimeMs},
         {"AvgQueueingLatencyMs", StatisticType::kAvgQueueingLatencyMs},
         {"AvgNetworkLatencyMs", StatisticType::kAvgNetworkLatencyMs},
         {"AvgPacketLatencyMs", StatisticType::kAvgPacketLatencyMs},
         {"AvgFrameLatencyMs", StatisticType::kAvgFrameLatencyMs},
         {"AvgEndToEndLatencyMs", StatisticType::kAvgEndToEndLatencyMs},
         {"EncodeRateKbps", StatisticType::kEncodeRateKbps},
         {"PacketTransmissionRateKbps",
          StatisticType::kPacketTransmissionRateKbps},
         {"TimeSinceLastReceiverResponseMs",
          StatisticType::kTimeSinceLastReceiverResponseMs},
         {"NumFramesCaptured", StatisticType::kNumFramesCaptured},
         {"NumFramesDroppedByEncoder",
          StatisticType::kNumFramesDroppedByEncoder},
         {"NumLateFrames", StatisticType::kNumLateFrames},
         {"NumPacketsSent", StatisticType::kNumPacketsSent},
         {"NumPacketsReceived", StatisticType::kNumPacketsReceived},
         {"FirstEventTimeMs", StatisticType::kFirstEventTimeMs},
         {"LastEventTimeMs", StatisticType::kLastEventTimeMs}}};

// External linkage for unit test
extern const EnumNameTable<HistogramType,
                           static_cast<size_t>(HistogramType::kNumTypes)>
    kHistogramTypeNames = {
        {{"CaptureLatencyMs", HistogramType::kCaptureLatencyMs},
         {"EncodeTimeMs", HistogramType::kEncodeTimeMs},
         {"QueueingLatencyMs", HistogramType::kQueueingLatencyMs},
         {"NetworkLatencyMs", HistogramType::kNetworkLatencyMs},
         {"PacketLatencyMs", HistogramType::kPacketLatencyMs},
         {"EndToEndLatencyMs", HistogramType::kEndToEndLatencyMs},
         {"FrameLatenessMs", HistogramType::kFrameLatenessMs}}};

SimpleHistogram::SimpleHistogram() = default;
SimpleHistogram::SimpleHistogram(int64_t min, int64_t max, int64_t width)

    : min(min), max(max), width(width), buckets((max - min) / width + 2) {
  OSP_CHECK_GT(buckets.size(), 2u);
  OSP_CHECK_EQ(0, (max - min) % width);
}

SimpleHistogram::SimpleHistogram(const SimpleHistogram&) = default;
SimpleHistogram::SimpleHistogram(SimpleHistogram&&) noexcept = default;
SimpleHistogram& SimpleHistogram::operator=(const SimpleHistogram&) = default;
SimpleHistogram& SimpleHistogram::operator=(SimpleHistogram&&) = default;
SimpleHistogram::~SimpleHistogram() = default;

bool SimpleHistogram::operator==(const SimpleHistogram& other) const {
  return min == other.min && max == other.max && width == other.width &&
         buckets == other.buckets;
}

void SimpleHistogram::Add(int64_t sample) {
  if (sample < min) {
    ++buckets.front();
  } else if (sample >= max) {
    ++buckets.back();
  } else {
    size_t index = 1 + (sample - min) / width;
    OSP_CHECK_LT(index, buckets.size());
    ++buckets[index];
  }
}

void SimpleHistogram::Reset() {
  buckets.assign(buckets.size(), 0);
}

Json::Value SimpleHistogram::ToJson() const {
  // Nest the bucket values in an array instead of a dictionary, so we sort
  // numerically instead of alphabetically.
  Json::Value out(Json::ValueType::arrayValue);
  for (size_t i = 0; i < buckets.size(); ++i) {
    if (buckets[i] != 0) {
      Json::Value entry;
      entry[GetBucketName(i)] = buckets[i];
      out.append(entry);
    }
  }
  return out;
}

std::string SimpleHistogram::ToString() const {
  return json::Stringify(ToJson()).value();
}

SimpleHistogram::SimpleHistogram(int64_t min,
                                 int64_t max,
                                 int64_t width,
                                 std::vector<int> buckets)
    : SimpleHistogram(min, max, width) {
  this->buckets = std::move(buckets);
}

std::string SimpleHistogram::GetBucketName(size_t index) const {
  if (index == 0) {
    return "<" + std::to_string(min);
  }

  if (index == buckets.size() - 1) {
    return ">=" + std::to_string(max);
  }

  // See the constructor comment for an example of how these bucket bounds
  // are calculated.
  const int bucket_min = min + width * (index - 1);
  const int bucket_max = min + index * width - 1;
  return StringPrintf("%d-%d", bucket_min, bucket_max);
}

Json::Value SenderStats::ToJson() const {
  Json::Value out;
  out["audio_statistics"] = ArrayToJson(audio_statistics, kStatisticTypeNames);
  out["audio_histograms"] = ArrayToJson(audio_histograms, kHistogramTypeNames);
  out["video_statistics"] = ArrayToJson(video_statistics, kStatisticTypeNames);
  out["video_histograms"] = ArrayToJson(video_histograms, kHistogramTypeNames);
  return out;
}

std::string SenderStats::ToString() const {
  return json::Stringify(ToJson()).value();
}

SenderStatsClient::~SenderStatsClient() {}

}  // namespace openscreen::cast
