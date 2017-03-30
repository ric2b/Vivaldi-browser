// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (C) 2015 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#include "media/filters/ipc_audio_decoder.h"

#include <algorithm>
#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "media/base/audio_bus.h"
#include "media/base/data_source.h"
#include "media/base/decoder_buffer.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_switches.h"
#include "media/base/platform_mime_util.h"
#include "media/base/sample_format.h"
#include "media/filters/ffmpeg_glue.h"
#include "media/filters/platform_media_pipeline_types.h"
#include "media/filters/protocol_sniffer.h"

#if defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#endif

namespace media {

namespace {

bool g_enabled = true;

base::LazyInstance<media::IPCMediaPipelineHost::Creator>::Leaky
    g_ipc_media_pipeline_host_creator = LAZY_INSTANCE_INITIALIZER;
base::LazyInstance<scoped_refptr<base::SequencedTaskRunner>>
    g_main_task_runner = LAZY_INSTANCE_INITIALIZER;
base::LazyInstance<scoped_refptr<base::SequencedTaskRunner>>
    g_media_task_runner = LAZY_INSTANCE_INITIALIZER;

void RunCreatorOnMainThread(
    DataSource* data_source,
    std::unique_ptr<IPCMediaPipelineHost>* ipc_media_pipeline_host) {
  *ipc_media_pipeline_host = g_ipc_media_pipeline_host_creator.Get()
                                .Run(g_media_task_runner.Get(), data_source);
}

void RunAndSignal(const base::Closure& task, base::WaitableEvent* done) {
  task.Run();
  done->Signal();
}

void PostTaskAndWait(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    const tracked_objects::Location& from_here,
    const base::Closure& task) {
  base::WaitableEvent done(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                           base::WaitableEvent::InitialState::NOT_SIGNALED);
  task_runner->PostTask(from_here, base::Bind(&RunAndSignal, task, &done));
  done.Wait();
}

}  // namespace

IPCAudioDecoder::ScopedDisableForTesting::ScopedDisableForTesting() {
  g_enabled = false;
}

IPCAudioDecoder::ScopedDisableForTesting::~ScopedDisableForTesting() {
  g_enabled = true;
}

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
      audio_bus_(nullptr),
      frames_read_(0),
      media_task_done_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                       base::WaitableEvent::InitialState::NOT_SIGNALED) {
  DCHECK(IsAvailable());
}

IPCAudioDecoder::~IPCAudioDecoder() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!ipc_media_pipeline_host_)
    return;

  PostTaskAndWait(g_media_task_runner.Get(), FROM_HERE,
                  base::Bind(&IPCMediaPipelineHost::Stop,
                             base::Unretained(ipc_media_pipeline_host_.get())));

  g_media_task_runner.Get()->DeleteSoon(FROM_HERE,
                                        ipc_media_pipeline_host_.release());
}

// static
bool IPCAudioDecoder::IsAvailable() {
  if (!g_enabled)
    return false;

#if defined(OS_MACOSX)
  if (!base::mac::IsOSYosemiteOrLater())
    // The pre-10.10 PlatformMediaPipeline implementation decodes media by
    // playing them at the regular playback rate.  This is unacceptable for Web
    // Audio API.
    return false;
#endif

  return IsPlatformMediaPipelineAvailable(media::PlatformMediaCheckType::BASIC);
}

// static
void IPCAudioDecoder::Preinitialize(
    const media::IPCMediaPipelineHost::Creator& ipc_media_pipeline_host_creator,
    const scoped_refptr<base::SequencedTaskRunner>& main_task_runner,
    const scoped_refptr<base::SequencedTaskRunner>& media_task_runner) {
  DCHECK(IsAvailable());
  DCHECK(!ipc_media_pipeline_host_creator.is_null());
  DCHECK(main_task_runner);
  DCHECK(media_task_runner);

  g_ipc_media_pipeline_host_creator.Get() = ipc_media_pipeline_host_creator;
  g_main_task_runner.Get() = main_task_runner;
  g_media_task_runner.Get() = media_task_runner;
}

bool IPCAudioDecoder::Initialize() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(g_media_task_runner.Get()) << "Must call Preinitialize() first";

  PostTaskAndWait(g_main_task_runner.Get(), FROM_HERE,
                  base::Bind(&RunCreatorOnMainThread, data_source_.get(),
                             &ipc_media_pipeline_host_));

  g_media_task_runner.Get()->PostTask(
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
  DCHECK(g_media_task_runner.Get()->RunsTasksOnCurrentThread());

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

int IPCAudioDecoder::Read(AudioBus* audio_bus) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!ipc_media_pipeline_host_ || audio_bus->channels() != channels_)
    return 0;

  audio_bus_ = audio_bus;

  g_media_task_runner.Get()->PostTask(
      FROM_HERE,
      base::Bind(&IPCAudioDecoder::ReadInternal, base::Unretained(this)));
  media_task_done_.Wait();

  return frames_read_;
}

void IPCAudioDecoder::ReadInternal() {
  DCHECK(g_media_task_runner.Get()->RunsTasksOnCurrentThread());

  ipc_media_pipeline_host_->ReadDecodedData(
      PLATFORM_MEDIA_AUDIO,
      base::Bind(&IPCAudioDecoder::DataReady, base::Unretained(this)));
}

void IPCAudioDecoder::DataReady(DemuxerStream::Status status,
                                const scoped_refptr<DecoderBuffer>& buffer) {
  DCHECK(g_media_task_runner.Get()->RunsTasksOnCurrentThread());

  switch (status) {
    case DemuxerStream::Status::kAborted:
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

      const int frame_count = std::min((int) buffer->data_size() / bytes_per_frame_,
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
      break;
  }
}

}  // namespace media
