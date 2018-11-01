// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (C) 2015 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#include "platform_media/common/pipeline_stats.h"

#include <map>
#include <set>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "media/base/demuxer_stream.h"
#include "platform_media/common/platform_media_pipeline_types.h"

namespace media {
namespace pipeline_stats {

namespace {

const char kStatusListPath[] = "platform_pipeline_status_list";

// Used in UMA histograms.  Don't remove or reorder values!
enum PipelineStatus {
  PIPELINE_INITIALIZED = 0,
  PIPELINE_INITIALIZED_HW = 1,
  PIPELINE_NOT_AVAILABLE = 2,
  PIPELINE_NO_GPU_PROCESS = 3,
  PIPELINE_INITIALIZE_ERROR = 4,
  PIPELINE_INITIALIZE_ERROR_HW = 5,
  PIPELINE_AUDIO_DECODE_ERROR = 6,
  PIPELINE_VIDEO_DECODE_ERROR = 7,
  PIPELINE_VIDEO_DECODE_ERROR_HW = 8,

  DECODER_AUDIO_INITIALIZED = 9,
  DECODER_AUDIO_INITIALIZE_ERROR = 10,
  DECODER_AUDIO_DECODE_ERROR = 11,
  DECODER_VIDEO_INITIALIZED = 12,
  DECODER_VIDEO_INITIALIZE_ERROR = 13,
  DECODER_VIDEO_DECODE_ERROR = 14,
  DECODER_NO_GPU_PROCESS = 15,

  PIPELINE_STATUS_COUNT
};

class StatusQueue {
 public:
  static void Serialize(const StatusQueue& queue,
                        base::DictionaryValue* dictionary);
  static std::vector<PipelineStatus> Deserialize(
      const base::DictionaryValue& dictionary);

  void Push(PipelineStatus status) { queue_.push_back(status); }
  void Clear() { queue_.clear(); }

 private:
  std::vector<PipelineStatus> queue_;
};

// static
void StatusQueue::Serialize(const StatusQueue& queue,
                            base::DictionaryValue* dictionary) {
  if (queue.queue_.empty())
    return;

  auto list = base::WrapUnique(new base::ListValue);
  for (const auto value : queue.queue_)
    list->AppendInteger(value);

  dictionary->SetWithoutPathExpansion(kStatusListPath, std::move(list));
}

// static
std::vector<PipelineStatus> StatusQueue::Deserialize(
    const base::DictionaryValue& dictionary) {
  std::vector<PipelineStatus> result;

  const base::ListValue* list;
  if (!dictionary.GetListWithoutPathExpansion(kStatusListPath, &list))
    return result;

  for (size_t i = 0; i < list->GetSize(); ++i) {
    int int_value;
    if (list->GetInteger(i, &int_value) && 0 <= int_value &&
        int_value < PIPELINE_STATUS_COUNT) {
      result.push_back(static_cast<PipelineStatus>(int_value));
    }
  }

  return result;
}

class StatsGlobalState {
public:
    static StatsGlobalState * GetInstance() {
        static StatsGlobalState * instance = new StatsGlobalState();
        return instance;
    }

    // Maps DemuxerStream instances to decoding mode.
    std::map<const DemuxerStream*, PlatformMediaDecodingMode> g_pipeline_streams;

    // A registry of decoder class names.
    std::set<std::string> g_decoder_class_names;

    // DemuxerStream instances associated with decoders (rather than the whole
    // pipeline).
    std::set<const DemuxerStream*> g_decoder_streams;

    // A queue of PipelineStatus values.  Used in a child process to collect stats
    // before sending them to the browser process.
    StatusQueue g_status_queue;
};

void Enqueue(PipelineStatus status) {
  StatsGlobalState::GetInstance()->g_status_queue.Push(status);
}

void ReportDecoderStreamError(const DemuxerStream* stream) {
  switch (stream->type()) {
    case DemuxerStream::AUDIO:
      Enqueue(DECODER_AUDIO_DECODE_ERROR);
      break;

    case DemuxerStream::VIDEO:
      Enqueue(DECODER_VIDEO_DECODE_ERROR);
      break;

    default:
      NOTREACHED();
      break;
  }
}

void ReportPipelineStreamError(const DemuxerStream* stream,
                               PlatformMediaDecodingMode decoding_mode) {
  switch (stream->type()) {
    case DemuxerStream::AUDIO:
      Enqueue(PIPELINE_AUDIO_DECODE_ERROR);
      break;

    case DemuxerStream::VIDEO: {
      Enqueue(decoding_mode == PlatformMediaDecodingMode::HARDWARE
                  ? PIPELINE_VIDEO_DECODE_ERROR_HW
                  : PIPELINE_VIDEO_DECODE_ERROR);
      break;
    }

    default:
      NOTREACHED();
      break;
  }
}

}  // namespace

void ReportNoPlatformSupport() {
  Enqueue(PIPELINE_NOT_AVAILABLE);
}

void ReportNoGpuProcess() {
  Enqueue(PIPELINE_NO_GPU_PROCESS);
}

void ReportNoGpuProcessForDecoder() {
  Enqueue(DECODER_NO_GPU_PROCESS);
}

void ReportStartResult(
    bool success,
    PlatformMediaDecodingMode attempted_video_decoding_mode) {
  if (success) {
    Enqueue(attempted_video_decoding_mode == PlatformMediaDecodingMode::HARDWARE
                ? PIPELINE_INITIALIZED_HW
                : PIPELINE_INITIALIZED);
    return;
  }

  Enqueue(attempted_video_decoding_mode == PlatformMediaDecodingMode::HARDWARE
              ? PIPELINE_INITIALIZE_ERROR_HW
              : PIPELINE_INITIALIZE_ERROR);
}

void ReportAudioDecoderInitResult(bool success) {
  Enqueue(success ? DECODER_AUDIO_INITIALIZED : DECODER_AUDIO_INITIALIZE_ERROR);
}

void ReportVideoDecoderInitResult(bool success) {
  Enqueue(success ? DECODER_VIDEO_INITIALIZED : DECODER_VIDEO_INITIALIZE_ERROR);
}

void AddStream(const DemuxerStream* stream,
               PlatformMediaDecodingMode decoding_mode) {
  DCHECK_EQ(0u, StatsGlobalState::GetInstance()->g_pipeline_streams.count(stream));
  StatsGlobalState::GetInstance()->g_pipeline_streams[stream] = decoding_mode;
}

void RemoveStream(const DemuxerStream* stream) {
  DCHECK_EQ(1u, StatsGlobalState::GetInstance()->g_pipeline_streams.count(stream));
  StatsGlobalState::GetInstance()->g_pipeline_streams.erase(stream);
}

void AddDecoderClass(const std::string& decoder_class_name) {
  StatsGlobalState::GetInstance()->g_decoder_class_names.insert(decoder_class_name);
}

void AddStreamForDecoderClass(const DemuxerStream* stream,
                              const std::string& decoder_class_name) {
  if (StatsGlobalState::GetInstance()->g_decoder_class_names.count(decoder_class_name) < 1)
    // Unknown decoder name -- no one claimed it by calling AddDecoderClass().
    return;

  const auto result = StatsGlobalState::GetInstance()->g_decoder_streams.insert(stream);
  DCHECK(result.second);
}

void RemoveStreamForDecoderClass(const DemuxerStream* stream,
                                 const std::string& decoder_class_name) {
  if (StatsGlobalState::GetInstance()->g_decoder_class_names.count(decoder_class_name) < 1)
    // Unknown decoder name -- no one claimed it by calling AddDecoderClass().
    return;

  const auto erased_count = StatsGlobalState::GetInstance()->g_decoder_streams.erase(stream);
  DCHECK_EQ(1u, erased_count);
}

void ReportStreamError(const DemuxerStream* stream) {
  if (StatsGlobalState::GetInstance()->g_decoder_streams.count(stream) > 0) {
    ReportDecoderStreamError(stream);
    return;
  }

  const auto it = StatsGlobalState::GetInstance()->g_pipeline_streams.find(stream);
  if (it != StatsGlobalState::GetInstance()->g_pipeline_streams.end()) {
    ReportPipelineStreamError(stream, it->second);
    return;
  }

  // Unknown DemuxerStream -- no one claimed it either by calling AddStream()
  // or AddStreamForDecoderClass().
}

void SerializeInto(base::DictionaryValue* dictionary) {
  StatusQueue::Serialize(StatsGlobalState::GetInstance()->g_status_queue, dictionary);
  StatsGlobalState::GetInstance()->g_status_queue.Clear();
}

void DeserializeAndReport(const base::DictionaryValue& dictionary) {
  const auto values = StatusQueue::Deserialize(dictionary);

  for (const auto value : values) {
    UMA_HISTOGRAM_ENUMERATION("Opera.DSK.Media.PlatformPipelineStatus", value,
                              PIPELINE_STATUS_COUNT);
  }
}

}  // namespace pipeline_stats
}  // namespace media
