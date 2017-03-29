// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (C) 2015 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#include "media/filters/ipc_audio_decoder.h"

#include <algorithm>

#include "base/callback_helpers.h"
#include "media/base/audio_bus.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/data_source.h"
#include "media/base/decoder_buffer.h"
#include "media/base/demuxer_stream.h"
#include "media/base/sample_format.h"
#include "media/filters/ffmpeg_glue.h"
#include "media/filters/platform_media_pipeline_types.h"
#include "media/filters/protocol_sniffer.h"

namespace media {

// An implementation of the DataSource interface that is a wrapper around
// FFmpegURLProtocol.
class IPCAudioDecoder::InMemoryDataSource : public media::DataSource {
 public:
  explicit InMemoryDataSource(FFmpegURLProtocol* protocol);
  ~InMemoryDataSource() override;

  // DataSource implementation.
  void Read(int64_t position,
            int size,
            uint8_t* data,
            const DataSource::ReadCB& read_cb) override;
  void Stop() override;
  bool GetSize(int64_t* size_out) override;
  bool IsStreaming() override;
  void SetBitrate(int bitrate) override;

  std::string mime_type() const { return mime_type_; }

 private:
  void OnMimeTypeSniffed(const std::string& mime_type);

  base::WaitableEvent mime_type_sniffed_event_;

  std::string mime_type_;
  FFmpegURLProtocol* protocol_;
  bool stopped_;

  DISALLOW_COPY_AND_ASSIGN(InMemoryDataSource);
};

IPCAudioDecoder::InMemoryDataSource::InMemoryDataSource(
    FFmpegURLProtocol* protocol)
    : mime_type_sniffed_event_(false, false),
      protocol_(protocol),
      stopped_(false) {
  DCHECK(protocol_);
  ProtocolSniffer protocol_sniffer;
  protocol_sniffer.SniffProtocol(
      this, base::Bind(&InMemoryDataSource::OnMimeTypeSniffed,
                       base::Unretained(this)));
  mime_type_sniffed_event_.Wait();
}

IPCAudioDecoder::InMemoryDataSource::~InMemoryDataSource() {
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

bool IPCAudioDecoder::InMemoryDataSource::GetSize(int64* size_out) {
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
  mime_type_sniffed_event_.Signal();
}

IPCAudioDecoder::IPCAudioDecoder(FFmpegURLProtocol* protocol)
    : data_source_(new InMemoryDataSource(protocol)),
      channels_(0),
      sample_rate_(0),
      number_of_frames_(0),
      bytes_per_frame_(0),
      sample_format_(kUnknownSampleFormat),
      initialized_successfully_(false),
      initialization_event_(false, false),
      destruction_event_(false, false),
      read_event_(false, false),
      audio_bus_(nullptr),
      frames_read_(0) {
}

IPCAudioDecoder::~IPCAudioDecoder() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (ipc_media_pipeline_host_.get()) {
    media_task_runner_.Get()->PostTask(
        FROM_HERE,
        base::Bind(&IPCMediaPipelineHost::Stop,
                   base::Unretained(ipc_media_pipeline_host_.get())));

    media_task_runner_.Get()->PostTask(
        FROM_HERE, base::Bind(&IPCAudioDecoder::DestroyPipelineOnMediaThread,
                              base::Unretained(this)));
    destruction_event_.Wait();
  }
}

void IPCAudioDecoder::DestroyPipelineOnMediaThread() {
  DCHECK(media_task_runner_.Get()->BelongsToCurrentThread());

  ipc_media_pipeline_host_.reset();
  destruction_event_.Signal();
}

bool IPCAudioDecoder::preinitialized_ = false;
base::LazyInstance<media::IPCMediaPipelineHost::Creator>::Leaky
    IPCAudioDecoder::ipc_media_pipeline_host_creator_ =
        LAZY_INSTANCE_INITIALIZER;
base::LazyInstance<scoped_refptr<base::SingleThreadTaskRunner>>::Leaky
    IPCAudioDecoder::media_task_runner_ = LAZY_INSTANCE_INITIALIZER;

void IPCAudioDecoder::Preinitialize(
    const media::IPCMediaPipelineHost::Creator& ipc_media_pipeline_host_creator,
    const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner) {
  ipc_media_pipeline_host_creator_.Get() = ipc_media_pipeline_host_creator;
  media_task_runner_.Get() = media_task_runner;
  preinitialized_ = true;
}

bool IPCAudioDecoder::Initialize() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!preinitialized_)
    return false;

  ipc_media_pipeline_host_ =
      ipc_media_pipeline_host_creator_.Get()
          .Run(media_task_runner_.Get(), data_source_.get())
          .Pass();
  media_task_runner_.Get()->PostTask(
      FROM_HERE, base::Bind(&IPCMediaPipelineHost::Initialize,
                            base::Unretained(ipc_media_pipeline_host_.get()),
                            data_source_->mime_type(),
                            base::Bind(&IPCAudioDecoder::OnInitialized,
                                       base::Unretained(this))));
  initialization_event_.Wait();
  return initialized_successfully_;
}

void IPCAudioDecoder::OnInitialized(bool success,
                                    int bitrate,
                                    const PlatformMediaTimeInfo& time_info,
                                    const PlatformAudioConfig& audio_config,
                                    const PlatformVideoConfig& video_config) {
  DCHECK(media_task_runner_.Get()->BelongsToCurrentThread());

  initialized_successfully_ = success && audio_config.is_valid();
  if (initialized_successfully_) {
    channels_ = audio_config.channel_count;
    sample_rate_ = audio_config.samples_per_second;
    number_of_frames_ = base::saturated_cast<int>(
        ceil(time_info.duration.InSecondsF() * sample_rate_));
    bytes_per_frame_ =
        channels_ * SampleFormatToBytesPerChannel(audio_config.format);
    sample_format_ = audio_config.format;
    duration_ = time_info.duration;
  }

  initialization_event_.Signal();
  return;
}

int IPCAudioDecoder::Read(AudioBus* audio_bus) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!initialized_successfully_ || audio_bus->channels() != channels_)
    return 0;

  audio_bus_ = audio_bus;
  frames_read_ = 0;
  media_task_runner_.Get()->PostTask(
      FROM_HERE,
      base::Bind(&IPCAudioDecoder::ReadInternal, base::Unretained(this)));
  read_event_.Wait();

  return frames_read_;
}

void IPCAudioDecoder::ReadInternal() {
  DCHECK(media_task_runner_.Get()->BelongsToCurrentThread());

  ipc_media_pipeline_host_->ReadDecodedData(
      PLATFORM_MEDIA_AUDIO,
      base::Bind(&IPCAudioDecoder::DataReady, base::Unretained(this)));
}

void IPCAudioDecoder::DataReady(DemuxerStream::Status status,
                                const scoped_refptr<DecoderBuffer>& buffer) {
  DCHECK(media_task_runner_.Get()->BelongsToCurrentThread());

  switch (status) {
    case DemuxerStream::Status::kAborted:
      frames_read_ = -1;
      read_event_.Signal();
      break;
    case DemuxerStream::Status::kConfigChanged:
      // When config changes the decoder buffer does not contain any useful
      // data, so we need to explicitly ask for more.
      ReadInternal();
      break;
    case DemuxerStream::Status::kOk:
      if (buffer->end_of_stream()) {
        read_event_.Signal();
      } else {
        const int frame_count = std::min(buffer->data_size() / bytes_per_frame_,
                                         audio_bus_->frames() - frames_read_);

        // Deinterleave each channel. The final format should be 32bit
        // floating-point planar.
        if (sample_format_ == kSampleFormatF32) {
          const float* decoded_audio_data =
              reinterpret_cast<const float*>(buffer->data());
          for (int channel_index = 0; channel_index < channels_;
               ++channel_index) {
            float* bus_data = audio_bus_->channel(channel_index) + frames_read_;
            for (int frame_index = 0, channel_offset = channel_index;
                 frame_index < frame_count;
                 ++frame_index, channel_offset += channels_) {
              bus_data[frame_index] = decoded_audio_data[channel_offset];
            }
          }
        } else if (sample_format_ == kSampleFormatPlanarF32) {
          const int channel_size = buffer->data_size() / channels_;
          for (int channel_index = 0; channel_index < channels_;
               ++channel_index) {
            memcpy(audio_bus_->channel(channel_index) + frames_read_,
                   buffer->data() + channel_index * channel_size,
                   sizeof(float) * frame_count);
          }
        } else {
          NOTREACHED();
        }

        frames_read_ += frame_count;

        ReadInternal();
      }
      break;
  }
}

}  // namespace media
