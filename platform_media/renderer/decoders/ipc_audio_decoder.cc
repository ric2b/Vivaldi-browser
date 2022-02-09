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
#include "base/logging.h"
#include "base/threading/thread_restrictions.h"
#include "media/base/audio_bus.h"
#include "media/base/data_source.h"
#include "media/base/decoder_buffer.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_switches.h"
#include "media/base/sample_format.h"
#include "media/filters/ffmpeg_glue.h"

#include "platform_media/common/platform_logging_util.h"
#include "platform_media/common/platform_media_pipeline_types.h"
#include "platform_media/renderer/decoders/ipc_factory.h"
#include "platform_media/renderer/pipeline/protocol_sniffer.h"

#if defined(OS_MAC)
#include "base/mac/mac_util.h"
#endif

namespace media {

namespace {

bool g_audio_decoder_available = true;

}  // namespace

// An implementation of the DataSource interface that is a wrapper around
// FFmpegURLProtocol.
class IPCAudioDecoder::InMemoryDataSource : public DataSource {
 public:
  explicit InMemoryDataSource(FFmpegURLProtocol* protocol);
  InMemoryDataSource(const InMemoryDataSource&) = delete;
  InMemoryDataSource& operator=(const InMemoryDataSource&) = delete;

  // DataSource implementation.
  void Read(int64_t position,
            int size,
            uint8_t* data,
            DataSource::ReadCB read_cb) override;
  void Stop() override;
  void Abort() override;
  bool GetSize(int64_t* size_out) override;
  bool IsStreaming() override;
  void SetBitrate(int bitrate) override;

  std::string mime_type() const { return mime_type_; }

 private:
  void OnMimeTypeSniffed(std::string mime_type);

  std::string mime_type_;
  FFmpegURLProtocol* protocol_;
  bool stopped_;
};

IPCAudioDecoder::InMemoryDataSource::InMemoryDataSource(
    FFmpegURLProtocol* protocol)
    : protocol_(protocol), stopped_(false) {
  DCHECK(protocol_);

  // These is a synchronous call as Read() below calls the callback
  // synchronously. Thus Unretained() is safe.
  ProtocolSniffer::SniffProtocol(
      this, base::BindOnce(&InMemoryDataSource::OnMimeTypeSniffed,
                           base::Unretained(this)));
}

void IPCAudioDecoder::InMemoryDataSource::Read(int64_t position,
                                               int size,
                                               uint8_t* data,
                                               DataSource::ReadCB read_cb) {
  if (stopped_ || size < 0 || position < 0) {
    std::move(read_cb).Run(kReadError);
    return;
  }

  // It is not clear if protocol_->Read() result can be used to detect EOF. So
  // use a workaround that assumes that when the size is known, any atttempt to
  // read past it gives EOF.
  int nread;
  int64_t data_size;
  if (protocol_->GetSize(&data_size) && data_size >= 0 &&
      position >= data_size) {
    nread = 0;
  } else {
    protocol_->SetPosition(position);
    nread = protocol_->Read(size, data);
  }
  std::move(read_cb).Run(nread);
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
    std::string mime_type) {
  mime_type_ = std::move(mime_type);
}

IPCAudioDecoder::ScopedDisableForTesting::ScopedDisableForTesting() {
  VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__;
  g_audio_decoder_available = false;
}

IPCAudioDecoder::ScopedDisableForTesting::~ScopedDisableForTesting() {
  VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__;
  g_audio_decoder_available = true;
}

// static
bool IPCAudioDecoder::IsAvailable() {
  if (!g_audio_decoder_available) {
    VLOG(2) << " PROPMEDIA(RENDERER) : " << __FUNCTION__ << ": No, disabled";
    return false;
  }
  return IPCMediaPipelineHost::IsAvailable();
}

IPCAudioDecoder::IPCAudioDecoder(FFmpegURLProtocol* protocol)
    : data_source_(std::make_unique<InMemoryDataSource>(protocol)),
      channels_(0),
      sample_rate_(0),
      number_of_frames_(0),
      bytes_per_frame_(0),
      sample_format_(kUnknownSampleFormat),
      decoded_audio_packets_(nullptr),
      frames_read_(0),
      async_task_done_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                       base::WaitableEvent::InitialState::NOT_SIGNALED) {
  DCHECK(IsAvailable());
}

IPCAudioDecoder::~IPCAudioDecoder() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__;

  if (!ipc_media_pipeline_host_)
    return;

  media_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&IPCAudioDecoder::FinishHostOnMediaThread,
                                std::move(data_source_),
                                std::move(ipc_media_pipeline_host_)));
}

// static
void IPCAudioDecoder::FinishHostOnMediaThread(
    std::unique_ptr<InMemoryDataSource> data_source,
    std::unique_ptr<IPCMediaPipelineHost> ipc_media_pipeline_host) {
  // Explicitly destruct the host before the data source destructor runs as the
  // host holds a pointer to the latter.
  ipc_media_pipeline_host.reset();
}

bool IPCAudioDecoder::Initialize() {
  VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);

  // TODO(igor@vivaldi.com): Use a worker sequence, not the global media thread,
  // as the pipeline host can use any sequence.
  media_task_runner_ = IPCFactory::instance()->GetHostIpcRunner();

  ipc_media_pipeline_host_ = std::make_unique<IPCMediaPipelineHost>();

  base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope scoped_wait;

  // Unretained() is safe as we synchronize on the signal.
  media_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&IPCMediaPipelineHost::Initialize,
                     base::Unretained(ipc_media_pipeline_host_.get()),
                     data_source_.get(), data_source_->mime_type(),
                     base::BindOnce(&IPCAudioDecoder::OnInitialized,
                                    base::Unretained(this))));
  async_task_done_.Wait();

  return ipc_media_pipeline_host_ != nullptr;
}

void IPCAudioDecoder::OnInitialized(bool success) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  const PlatformAudioConfig& audio_config =
      ipc_media_pipeline_host_->audio_config();
  VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
          << Loggable(audio_config);

  if (success && audio_config.is_valid()) {
    channels_ = audio_config.channel_count;
    sample_rate_ = audio_config.samples_per_second;
    duration_ = ipc_media_pipeline_host_->time_info().duration;
    number_of_frames_ =
        base::saturated_cast<int>(ceil(duration_.InSecondsF() * sample_rate_));
    bytes_per_frame_ =
        channels_ * SampleFormatToBytesPerChannel(audio_config.format);
    sample_format_ = audio_config.format;
  } else {
    // The host explicitly allow to delete it during the initialize callback
    // call.
    ipc_media_pipeline_host_.reset();
  }

  async_task_done_.Signal();
}

int IPCAudioDecoder::Read(
    std::vector<std::unique_ptr<AudioBus>>* decoded_audio_packets) {
  VLOG(7) << " PROPMEDIA(RENDERER) : " << __FUNCTION__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);

  if (!ipc_media_pipeline_host_)
    return 0;

  decoded_audio_packets_ = decoded_audio_packets;

  base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope scoped_wait;

  // Unretained() is safe as we synchronize on the signal.
  media_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&IPCAudioDecoder::ReadInternal, base::Unretained(this)));
  async_task_done_.Wait();

  return frames_read_;
}

void IPCAudioDecoder::ReadInternal() {
  VLOG(7) << " PROPMEDIA(RENDERER) : " << __FUNCTION__;
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  // Unretained(this) is safe as the decoder must be waiting in
  // IPCAudioDecoder::Read for the signal.
  ipc_media_pipeline_host_->ReadDecodedData(
      PlatformStreamType::kAudio,
      base::BindRepeating(&IPCAudioDecoder::DataReady, base::Unretained(this)));
}

void IPCAudioDecoder::DataReady(DemuxerStream::Status status,
                                scoped_refptr<DecoderBuffer> buffer) {
  VLOG(7) << " PROPMEDIA(RENDERER) : " << __FUNCTION__;
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  switch (status) {
    case DemuxerStream::Status::kAborted:
    case DemuxerStream::Status::kError:
      frames_read_ = -1;
      async_task_done_.Signal();
      break;

    case DemuxerStream::Status::kConfigChanged:
      // When config changes the decoder buffer does not contain any useful
      // data, so we need to explicitly ask for more.
      ReadInternal();
      break;

    case DemuxerStream::Status::kOk:
      if (buffer->end_of_stream()) {
        async_task_done_.Signal();
        break;
      }

      int frames_in_buffer = ((int)buffer->data_size() / bytes_per_frame_);
      int frames_still_pending = (number_of_frames_ - frames_read_);

      const int frame_count = std::min(frames_in_buffer, frames_still_pending);

      if (frame_count > 0) {
        decoded_audio_packets_->emplace_back(
            AudioBus::Create(channels_, frame_count));
        AudioBus* audio_bus = decoded_audio_packets_->back().get();

        // Deinterleave each channel. The final format should be 32bit
        // floating-point planar.
        if (sample_format_ == SampleFormat::kSampleFormatF32) {
          const float* decoded_audio_data =
              reinterpret_cast<const float*>(buffer->data());
          for (int channel_index = 0; channel_index < channels_;
               ++channel_index) {
            float* bus_data = audio_bus->channel(channel_index);
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
