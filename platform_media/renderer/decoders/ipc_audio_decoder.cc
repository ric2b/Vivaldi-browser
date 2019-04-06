// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2015 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#include "platform_media/renderer/decoders/ipc_audio_decoder.h"

#include <algorithm>
#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/threading/thread_restrictions.h"
#include "media/base/audio_bus.h"
#include "media/base/data_source.h"
#include "media/base/decoder_buffer.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_switches.h"
#include "media/base/sample_format.h"
#include "media/filters/ffmpeg_glue.h"

#include "platform_media/common/platform_logging_util.h"
#include "platform_media/common/platform_mime_util.h"
#include "platform_media/common/platform_media_pipeline_types.h"
#include "platform_media/renderer/pipeline/protocol_sniffer.h"

#if defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#endif

namespace media {

// An implementation of the DataSource interface that is a wrapper around
// FFmpegURLProtocol.
class IPCAudioDecoder::InMemoryDataSource : public DataSource {
 public:
  explicit InMemoryDataSource(FFmpegURLProtocol* protocol);

  // DataSource implementation.
  void Read(int64_t position,
            int size,
            uint8_t* data,
            const DataSource::ReadCB& read_cb) override;
  void Stop() override;
  void Abort() override;
  bool GetSize(int64_t* size_out) override;
  bool IsStreaming() override;
  void SetBitrate(int bitrate) override;

  std::string mime_type() const { return mime_type_; }

 private:
  void OnMimeTypeSniffed(const std::string& mime_type);

  std::string mime_type_;
  FFmpegURLProtocol* protocol_;
  bool stopped_;

  DISALLOW_COPY_AND_ASSIGN(InMemoryDataSource);
};

IPCAudioDecoder::InMemoryDataSource::InMemoryDataSource(
    FFmpegURLProtocol* protocol)
    : protocol_(protocol), stopped_(false) {
  DCHECK(protocol_);

  ProtocolSniffer protocol_sniffer;
  protocol_sniffer.SniffProtocol(
      this, base::Bind(&InMemoryDataSource::OnMimeTypeSniffed,
                       base::Unretained(this)));
}

void IPCAudioDecoder::InMemoryDataSource::Read(
    int64_t position,
    int size,
    uint8_t* data,
    const DataSource::ReadCB& read_cb) {
  if (stopped_ || size < 0 || position < 0) {
    read_cb.Run(kReadError);
    return;
  }

  protocol_->SetPosition(position);
  read_cb.Run(protocol_->Read(size, data));
}

void IPCAudioDecoder::InMemoryDataSource::Stop() {
  stopped_ = true;
}

void IPCAudioDecoder::InMemoryDataSource::Abort() {
  // Do nothing.
}

bool IPCAudioDecoder::InMemoryDataSource::GetSize(int64_t* size_out) {
  return protocol_->GetSize(size_out);
}

bool IPCAudioDecoder::InMemoryDataSource::IsStreaming() {
  return protocol_->IsStreaming();
}

void IPCAudioDecoder::InMemoryDataSource::SetBitrate(int bitrate) {
  // Do nothing.
}

void IPCAudioDecoder::InMemoryDataSource::OnMimeTypeSniffed(
    const std::string& mime_type) {
  mime_type_ = mime_type;
}

IPCAudioDecoder::IPCAudioDecoder(FFmpegURLProtocol* protocol)
    : data_source_(new InMemoryDataSource(protocol)),
      channels_(0),
      sample_rate_(0),
      number_of_frames_(0),
      bytes_per_frame_(0),
      sample_format_(kUnknownSampleFormat),
      decoded_audio_packets_(nullptr),
      frames_read_(0),
      media_task_done_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                       base::WaitableEvent::InitialState::NOT_SIGNALED) {
  DCHECK(factory_.IsAvailable());
}

IPCAudioDecoder::~IPCAudioDecoder() {
  DCHECK(thread_checker_.CalledOnValidThread());
  VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__;

  if (!ipc_media_pipeline_host_)
    return;

  base::ThreadRestrictions::ScopedAllowWait scoped_wait;

  factory_.PostTaskAndWait(factory_.MediaTaskRunner(), FROM_HERE,
                  base::Bind(&IPCMediaPipelineHost::Stop,
                             base::Unretained(ipc_media_pipeline_host_.get())));

  factory_.MediaTaskRunner()->DeleteSoon(FROM_HERE,
                                        ipc_media_pipeline_host_.release());
}

void IPCAudioDecoder::RunCreatorOnMainThread(
        DataSource* data_source,
        std::unique_ptr<IPCMediaPipelineHost>* ipc_media_pipeline_host) {
  VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__;
  factory_.RunCreatorOnMainThread(data_source, ipc_media_pipeline_host);
}

bool IPCAudioDecoder::Initialize() {
  VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(factory_.MediaTaskRunner()) << "Must call Preinitialize() first";

  base::ThreadRestrictions::ScopedAllowWait scoped_wait;

  factory_.PostTaskAndWait(factory_.MainTaskRunner(), FROM_HERE,
                  base::Bind(&IPCAudioDecoder::RunCreatorOnMainThread,
                             base::Unretained(this),
                             data_source_.get(),
                             &ipc_media_pipeline_host_));

  factory_.MediaTaskRunner()->PostTask(
      FROM_HERE, base::Bind(&IPCMediaPipelineHost::Initialize,
                            base::Unretained(ipc_media_pipeline_host_.get()),
                            data_source_->mime_type(),
                            base::Bind(&IPCAudioDecoder::OnInitialized,
                                       base::Unretained(this))));
  media_task_done_.Wait();

  return ipc_media_pipeline_host_ != nullptr;
}

void IPCAudioDecoder::OnInitialized(
    bool success,
    int bitrate,
    const PlatformMediaTimeInfo& time_info,
    const PlatformAudioConfig& audio_config,
    const PlatformVideoConfig& /* video_config */) {
  DCHECK(factory_.MediaTaskRunner()->RunsTasksInCurrentSequence());

  VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
          << Loggable(audio_config);

  if (success && audio_config.is_valid()) {
    channels_ = audio_config.channel_count;
    sample_rate_ = audio_config.samples_per_second;
    number_of_frames_ = base::saturated_cast<int>(
        ceil(time_info.duration.InSecondsF() * sample_rate_));
    bytes_per_frame_ =
        channels_ * SampleFormatToBytesPerChannel(audio_config.format);
    sample_format_ = audio_config.format;
    duration_ = time_info.duration;
  } else {
    ipc_media_pipeline_host_.reset();
  }

  media_task_done_.Signal();
}


int IPCAudioDecoder::Read(
        std::vector<std::unique_ptr<AudioBus>>* decoded_audio_packets) {
  VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!ipc_media_pipeline_host_)
    return 0;

  decoded_audio_packets_ = decoded_audio_packets;

  base::ThreadRestrictions::ScopedAllowWait scoped_wait;

  factory_.MediaTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&IPCAudioDecoder::ReadInternal, base::Unretained(this)));
  media_task_done_.Wait();

  return frames_read_;
}

void IPCAudioDecoder::ReadInternal() {
  VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__;
  DCHECK(factory_.MediaTaskRunner()->RunsTasksInCurrentSequence());

  ipc_media_pipeline_host_->ReadDecodedData(
      PlatformMediaDataType::PLATFORM_MEDIA_AUDIO,
      base::Bind(&IPCAudioDecoder::DataReady, base::Unretained(this)));
}

void IPCAudioDecoder::DataReady(DemuxerStream::Status status,
                                scoped_refptr<DecoderBuffer> buffer) {
  VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__;
  DCHECK(factory_.MediaTaskRunner()->RunsTasksInCurrentSequence());

  switch (status) {
    case DemuxerStream::Status::kAborted:
    case DemuxerStream::Status::kError:
      frames_read_ = -1;
      media_task_done_.Signal();
      break;

    case DemuxerStream::Status::kConfigChanged:
      // When config changes the decoder buffer does not contain any useful
      // data, so we need to explicitly ask for more.
      ReadInternal();
      break;

    case DemuxerStream::Status::kOk:
      if (buffer->end_of_stream()) {
        media_task_done_.Signal();
        break;
      }

      int frames_in_buffer = ((int) buffer->data_size() / bytes_per_frame_);
      int frames_still_pending = (number_of_frames_ - frames_read_);

      const int frame_count = std::min(frames_in_buffer, frames_still_pending);

      if (frame_count > 0) {

        decoded_audio_packets_->emplace_back(
            AudioBus::Create(channels_, frame_count));
        AudioBus *audio_bus = decoded_audio_packets_->back().get();

        // Deinterleave each channel. The final format should be 32bit
        // floating-point planar.
        if (sample_format_ == SampleFormat::kSampleFormatF32) {
          const float *decoded_audio_data =
              reinterpret_cast<const float *>(buffer->data());
          for (int channel_index = 0; channel_index < channels_;
               ++channel_index) {
            float *bus_data = audio_bus->channel(channel_index);
            for (int frame_index = 0, channel_offset = channel_index;
                 frame_index < frame_count;
                 ++frame_index, channel_offset += channels_) {
              bus_data[frame_index] = decoded_audio_data[channel_offset];
            }
          }
        } else if (sample_format_ == SampleFormat::kSampleFormatPlanarF32) {
          const int channel_size = buffer->data_size() / channels_;
          for (int channel_index = 0; channel_index < channels_;
               ++channel_index) {
            memcpy(audio_bus->channel(channel_index),
                   buffer->data() + channel_index * channel_size,
                   sizeof(float) * frame_count);
          }
        } else {
          NOTREACHED();
        }

        frames_read_ += frame_count;
      }

      ReadInternal();
      break;
  }
}

}  // namespace media
