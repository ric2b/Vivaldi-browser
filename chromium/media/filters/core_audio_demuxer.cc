// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#include "media/filters/core_audio_demuxer.h"

#include <string>

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/task_runner_util.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/mac/scoped_audio_queue_ref.h"
#include "media/filters/blocking_url_protocol.h"
#include "media/filters/core_audio_demuxer_stream.h"
#include "net/base/mime_util.h"
#include "url/gurl.h"

namespace {

static const char* kSupportedMimeTypes[] = {
    "audio/aac",
    "audio/aacp",
    "audio/3gpp",
    "audio/3gpp2",
    "audio/mp3",
    "audio/mp4",
    "audio/mpeg",
    "audio/mpeg3"
};

}  // namespace

namespace media {

CoreAudioDemuxer::CoreAudioDemuxer(DataSource* data_source)
    : host_(NULL),
      data_source_(data_source),
      blocking_thread_("CoreAudioDemuxer"),
      bit_rate_(0),
      input_format_found_(false),
      weak_factory_(this) {
  DCHECK(data_source_);

  url_protocol_.reset(new BlockingUrlProtocol(
      data_source_,
      BindToCurrentLoop(base::Bind(&CoreAudioDemuxer::OnDataSourceError,
                                   base::Unretained(this)))));
}

CoreAudioDemuxer::~CoreAudioDemuxer() {
}

std::string CoreAudioDemuxer::GetDisplayName() const {
  return "CoreAudioDemuxer";
}

void CoreAudioDemuxer::Initialize(DemuxerHost* host,
                                  const PipelineStatusCB& status_cb,
                                  bool enable_text_tracks) {
  host_ = host;

  CHECK(blocking_thread_.Start());
  ReadAudioFormatInfo(status_cb);
}

CoreAudioDemuxerStream* CoreAudioDemuxer::CreateAudioDemuxerStream() {
  return new CoreAudioDemuxerStream(
      this, input_format_info_, bit_rate_, CoreAudioDemuxerStream::AUDIO);
}

void CoreAudioDemuxer::StartWaitingForSeek(base::TimeDelta seek_time) {
}

void CoreAudioDemuxer::CancelPendingSeek(base::TimeDelta seek_time) {
}

void CoreAudioDemuxer::Seek(base::TimeDelta time,
                            const PipelineStatusCB& status_cb) {
  if (audio_stream_->Seek(time)) {
    status_cb.Run(PIPELINE_OK);
    return;
  }
  status_cb.Run(PIPELINE_ERROR_ABORT);
}

void CoreAudioDemuxer::Stop() {
  url_protocol_->Abort();

  data_source_->Stop();

  if (audio_stream_.get()) {
    audio_stream_->Stop();
  }

  // This will block until all tasks complete.
  blocking_thread_.Stop();

  data_source_ = NULL;
}

DemuxerStream* CoreAudioDemuxer::GetStream(DemuxerStream::Type type) {
  switch (type) {
    case DemuxerStream::AUDIO:
      return audio_stream_.get();

    default:
      return NULL;
  }
}

base::TimeDelta CoreAudioDemuxer::GetStartTime() const {
  // TODO(wdzierzanowski): Fetch actual start time from media file (DNA-27693).
  return base::TimeDelta();
}

base::Time CoreAudioDemuxer::GetTimelineOffset() const {
  return base::Time();
}

int64_t CoreAudioDemuxer::GetMemoryUsage() const {
  // TODO(ckulakowski): Implement me. DNA-45936
  return 0;
}


void CoreAudioDemuxer::SetAudioDuration(int64_t duration) {
  host_->SetDuration(base::TimeDelta::FromMilliseconds(duration));
}

void CoreAudioDemuxer::ReadDataSourceWithCallback(
    const DataSource::ReadCB& read_cb) {
  base::PostTaskAndReplyWithResult(
      blocking_thread_.task_runner().get(),
      FROM_HERE,
      base::Bind(&CoreAudioDemuxer::ReadDataSource, base::Unretained(this)),
      read_cb);
}

void CoreAudioDemuxer::ReadAudioFormatInfo(const PipelineStatusCB& status_cb) {
  ReadDataSourceWithCallback(
      base::Bind(&CoreAudioDemuxer::OnReadAudioFormatInfoDone,
                 weak_factory_.GetWeakPtr(),
                 status_cb));
}

void CoreAudioDemuxer::OnReadAudioFormatInfoDone(
    const PipelineStatusCB& status_cb,
    int read_size) {
  if (!blocking_thread_.IsRunning()) {
    status_cb.Run(PIPELINE_ERROR_ABORT);
    return;
  }

  if (read_size <= 0) {
    status_cb.Run(DEMUXER_ERROR_COULD_NOT_OPEN);
    return;
  }

  OSStatus err =
      AudioFileStreamOpen(this,
                          CoreAudioDemuxer::AudioPropertyListenerProc,
                          CoreAudioDemuxer::AudioPacketsProc,
                          kAudioFileMP3Type,
                          &audio_stream_id_);
  if (err == noErr) {
    err = AudioFileStreamParseBytes(audio_stream_id_, read_size, buffer_, 0);
    AudioFileStreamClose(audio_stream_id_);

    // If audio format is not known yet, demuxer must read more data to figure
    // it out.
    if (!input_format_found_) {
      ReadAudioFormatInfo(status_cb);
      return;
    }
  }

  if (err != noErr)
    status_cb.Run(PIPELINE_ERROR_ABORT);

  if (input_format_found_) {
    audio_stream_.reset(CreateAudioDemuxerStream());
    if (!audio_stream_->audio_decoder_config().IsValidConfig()) {
      audio_stream_.reset();
      status_cb.Run(DEMUXER_ERROR_NO_SUPPORTED_STREAMS);
      return;
    }

    // Reset read offset to the beginning.
    ResetDataSourceOffset();
    status_cb.Run(PIPELINE_OK);
  }
}

void CoreAudioDemuxer::OnDataSourceError() {
  host_->OnDemuxerError(PIPELINE_ERROR_READ);
}

void CoreAudioDemuxer::AudioPacketsProc(
    void* client_data,
    UInt32 number_bytes,
    UInt32 number_packets,
    const void* input_data,
    AudioStreamPacketDescription* packet_descriptions) {
  CoreAudioDemuxer* demuxer = reinterpret_cast<CoreAudioDemuxer*>(client_data);
  if (!demuxer->input_format_found_)
      return;

  UInt32 bit_rate_size = sizeof(UInt32);
  OSStatus err = AudioFileStreamGetProperty(demuxer->audio_stream_id_,
                                            kAudioFileStreamProperty_BitRate,
                                            &bit_rate_size,
                                            &demuxer->bit_rate_);
  if (err == noErr) {
    int64_t duration = 0;
    int64_t ds_size = 0;
    demuxer->data_source_->GetSize(&ds_size);
    if (demuxer->bit_rate_ > 0) {
      // some audio files gives bit_rate in 1000*bits/s, but others gives bits/s
      // According to the ISO standard, decoders are only required to be able
      // to decode streams up to 320, so it should be safe to calculate like
      // below
      if (demuxer->bit_rate_ >= 320)
        demuxer->bit_rate_ /= 1000;
      duration = (ds_size * 8) / demuxer->bit_rate_;
      demuxer->bit_rate_ *= 1024;
      demuxer->data_source_->SetBitrate(demuxer->bit_rate_);
    }

    VLOG(3) << "Audio bit rate: " << demuxer->bit_rate_
            << ", Duration: " << duration
            << ", Audio data source size: " << ds_size;
    demuxer->SetAudioDuration(duration);
  } else {
    // We are unable to find audio length. User will be still able to play,
    // but there is impossible to seek or display audio length in html control
    DLOG(WARNING) << "Cannot calculate audio duration";
  }
}

void CoreAudioDemuxer::AudioPropertyListenerProc(
    void* client_data,
    AudioFileStreamID audio_file_stream,
    AudioFileStreamPropertyID property_id,
    UInt32* io_flags) {
  // This is called by audio file stream when it finds property values.
  CoreAudioDemuxer* demuxer = reinterpret_cast<CoreAudioDemuxer*>(client_data);
  OSStatus err = noErr;

  char buf[] = {(static_cast<char>(property_id >> 24) & 255),
                (static_cast<char>(property_id >> 16) & 255),
                (static_cast<char>(property_id >> 8) & 255),
                (static_cast<char>(property_id & 255)), 0};
  VLOG(1) << "Found stream property " << buf;

  switch (property_id) {
    case kAudioFileStreamProperty_ReadyToProducePackets: {
      VLOG(3) << "Ready to produce packets";
      UInt32 asbd_size = sizeof(demuxer->input_format_info_);
      err = AudioFileStreamGetProperty(audio_file_stream,
                                       kAudioFileStreamProperty_DataFormat,
                                       &asbd_size,
                                       &demuxer->input_format_info_);
      if (err)
        LOG(ERROR) << "Get kAudioFileStreamProperty_DataFormat " << err;

      demuxer->input_format_found_ = true;
      break;
    }
  }
}

int CoreAudioDemuxer::ReadDataSource() {
  int64_t offset = 0;
  url_protocol_->GetPosition(&offset);
  VLOG(1) << "ReadDataSource: at offset: " << offset;
  return url_protocol_->Read(kStreamInfoBufferSize, buffer_);
}

void CoreAudioDemuxer::ResetDataSourceOffset() {
  url_protocol_->SetPosition(0);
}

void CoreAudioDemuxer::ReadDataSourceIfNeeded() {
  // Make sure we have work to do before reading.
  if (!blocking_thread_.IsRunning()) {
    audio_stream_->Abort();
    return;
  }

  ReadDataSourceWithCallback(base::Bind(&CoreAudioDemuxer::OnReadDataSourceDone,
                                        weak_factory_.GetWeakPtr()));
}

void CoreAudioDemuxer::OnReadDataSourceDone(int read_size) {
  audio_stream_->ReadCompleted(buffer_, read_size);
}

// static
bool CoreAudioDemuxer::IsSupported(const std::string& content_type,
                                   const GURL& url) {
  std::string mime_type = base::ToLowerASCII(content_type);
  if (mime_type.empty()) {
    base::FilePath file(url.ExtractFileName());
    if (!net::GetMimeTypeFromFile(file, &mime_type))
      return false;
  }
  for (size_t i = 0; i < arraysize(kSupportedMimeTypes); i++) {
    if (!mime_type.compare(kSupportedMimeTypes[i]))
      return true;
  }
  return false;
}

}  // namespace media
