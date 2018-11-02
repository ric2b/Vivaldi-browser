// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#include "platform_media/renderer/decoders/mac/core_audio_demuxer_stream.h"

#include <vector>

#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_util.h"
#include "media/base/video_decoder_config.h"
#include "platform_media/common/platform_logging_util.h"
#include "platform_media/renderer/decoders/mac/core_audio_demuxer.h"

namespace media {

namespace {

const size_t kAudioQueueMaxPacketDescs = 512;  // Number of packet descriptions.
const size_t kAudioQueueBufSize = 8 * CoreAudioDemuxer::kStreamInfoBufferSize;

const char* StringFromAudioError(OSStatus err) {
  switch (err) {
    case kAudioFileStreamError_UnsupportedFileType:
      return "The specified file type is not supported.";

    case kAudioFileStreamError_UnsupportedDataFormat:
      return "The data format is not supported by the specified file type.";

    case kAudioFileStreamError_UnsupportedProperty:
      return "The property is not supported.";

    case kAudioFileStreamError_BadPropertySize:
      return "The size of the buffer you provided for property data was not "
             "correct.";

    case kAudioFileStreamError_NotOptimized:
      return "It is not possible to produce output packets because the streamed"
             " audio file's packet table or other defining information is not "
             "present or appears after the audio data.";

    case kAudioFileStreamError_InvalidPacketOffset:
      return "A packet offset was less than 0, or past the end of the file, or "
             "a corrupt packet size was read when building the packet table.";

    case kAudioFileStreamError_InvalidFile:
      return "The file is malformed, not a valid instance of an audio file of "
             "its type, or not recognized as an audio file.";

    case kAudioFileStreamError_ValueUnknown:
      return "The property value is not present in this file before the audio "
             "data.";

    case kAudioFileStreamError_DataUnavailable:
      return "The amount of data provided to the parser was insufficient to "
             "produce any result.";

    case kAudioFileStreamError_IllegalOperation:
      return "An illegal operation was attempted.";

    case kAudioFileStreamError_UnspecifiedError:
      return "An unspecified error has occurred.";

    case kAudioFileStreamError_DiscontinuityCantRecover:
      return "A discontinuity has occurred in the audio data, and Audio File "
             "Stream Services cannot recover.";

    case kAudioQueueErr_InvalidBuffer:
      return "The specified audio queue buffer does not belong to the specified"
             " audio queue.";

    case kAudioQueueErr_BufferEmpty:
      return "The audio queue buffer is empty (that is, the mAudioDataByteSize "
             "field = 0).";

    case kAudioQueueErr_DisposalPending:
      return "The function cannot act on the audio queue because it is being "
             "asynchronously disposed of.";

    case kAudioQueueErr_InvalidProperty:
      return "The specified property ID is invalid.";

    case kAudioQueueErr_InvalidPropertySize:
      return "The size of the specified property is invalid.";

    case kAudioQueueErr_InvalidParameter:
      return "The specified parameter ID is invalid.";

    default:
      return "Unknown";
  }
}

}  //  namespace

CoreAudioDemuxerStream::CoreAudioDemuxerStream(
    CoreAudioDemuxer* demuxer,
    AudioStreamBasicDescription input_format,
    UInt32 bit_rate,
    Type type)
    : demuxer_(demuxer),
      is_enabled_(true),
      reading_audio_data_(false),
      is_enqueue_running_(false),
      output_buffer_(NULL),
      input_format_(input_format),
      audio_file_stream_(NULL),
      audio_queue_buffer_(NULL),
      bytes_filled_(0),
      packets_filled_(0),
      frames_decoded_(0),
      decoded_data_buffer_size_(0),
      bit_rate_(bit_rate),
      pending_seek_(false) {
  DCHECK_EQ(type, AUDIO);
  DCHECK(demuxer_);

  packet_descs_.reset(
      new std::vector<AudioStreamPacketDescription>(kAudioQueueMaxPacketDescs));

  memset(&output_format_, 0, sizeof(output_format_));

  time_stamp_.mFlags = kAudioTimeStampSampleTimeValid;
  time_stamp_.mSampleTime = 0;

  InitializeAudioDecoderConfig();

  OSStatus err = AudioFileStreamOpen(
      this, &CoreAudioDemuxerStream::AudioPropertyListenerProc,
      &CoreAudioDemuxerStream::AudioPacketsProc, kAudioFileMP3Type,
      &audio_file_stream_);
  CHECK(!err);

  err = AudioQueueNewOutput(
      &input_format_, CoreAudioDemuxerStream::AudioQueueOutputCallback, NULL,
      NULL, kCFRunLoopCommonModes, 0, audio_queue_.InitializeInto());

  if (err) {
    LOG(ERROR) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
               << " AudioQueueNewOutput, error = " << StringFromAudioError(err);
    audio_queue_.reset();
    return;
  }

  err = AudioQueueAllocateBuffer(audio_queue_, kAudioQueueBufSize,
                                 &audio_queue_buffer_);
  if (err) {
    LOG(ERROR) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
               << " AudioQueueAllocateBuffer, error = "
               << StringFromAudioError(err);
    audio_queue_.reset();
  }
}

CoreAudioDemuxerStream::~CoreAudioDemuxerStream() {
  DCHECK(read_cb_.is_null());

  AudioFileStreamClose(audio_file_stream_);

  if (audio_queue_) {
    AudioQueueStop(audio_queue_, true);
    AudioQueueFlush(audio_queue_);
  }
}

void CoreAudioDemuxerStream::InitializeAudioDecoderConfig() {
  ChannelLayout channel_layout = CHANNEL_LAYOUT_STEREO;
  if (input_format_.mChannelsPerFrame == 1)
    channel_layout = CHANNEL_LAYOUT_MONO;

  std::vector<uint8> extra_data;
  audio_config_.Initialize(AudioCodec::kCodecPCM,
                           SampleFormat::kSampleFormatS16,
                           channel_layout,
                           input_format_.mSampleRate,
                           extra_data,
                           Unencrypted(),
                           base::TimeDelta(),
                           0);
  VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
          << " New AudioDecoderConfig :"
          << Loggable(audio_config_);
}

void CoreAudioDemuxerStream::Read(const ReadCB& read_cb) {
  CHECK(read_cb_.is_null()) << "Overlapping reads are not supported";
  read_cb_ = read_cb;

  if (!audio_queue_) {
    base::ResetAndReturn(&read_cb_).Run(kAborted, NULL);
    return;
  }

  if (!is_enabled_) {
    VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
            << " Read from disabled stream, returning EOS";
    base::ResetAndReturn(&read_cb_).Run(kOk, DecoderBuffer::CreateEOSBuffer());
    return;
  }

  demuxer_->ReadDataSourceIfNeeded();
}

void CoreAudioDemuxerStream::ReadCompleted(uint8_t* read_data, int read_size) {
  if (read_cb_.is_null())
    return;

  if (read_size <= 0) {
    Stop();
    return;
  }

  int flags = pending_seek_ ? kAudioFileStreamParseFlag_Discontinuity : 0;
  OSStatus err = AudioFileStreamParseBytes(
      audio_file_stream_, read_size, read_data, flags);
  pending_seek_ = false;
  if (err != noErr) {
    LOG(ERROR) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
               << " AudioFileStreamParseBytes error: " << err;
    base::ResetAndReturn(&read_cb_).Run(kAborted, NULL);
    return;
  }

  if (!reading_audio_data_) {
    // The parser has not parsed up to the beginning of the audio data yet,
    // i.e., it is still reading tags, cover art, etc.  We ignore the non-audio
    // data and return an empty buffer.  We will continue parsing with the next
    // call to |Read()|.
    base::ResetAndReturn(&read_cb_).Run(kOk, new DecoderBuffer(0));
    return;
  }

  if (EnqueueBuffer() != noErr) {
    base::ResetAndReturn(&read_cb_).Run(kAborted, NULL);
    return;
  }

  base::ResetAndReturn(&read_cb_)
      .Run(kOk, DecoderBuffer::CopyFrom(
                    reinterpret_cast<uint8_t*>(output_buffer_->mAudioData),
                    frames_decoded_));
}

AudioDecoderConfig CoreAudioDemuxerStream::audio_decoder_config() {
  if (!audio_config_.IsValidConfig())
    InitializeAudioDecoderConfig();

  return audio_config_;
}

VideoDecoderConfig CoreAudioDemuxerStream::video_decoder_config() {
  NOTREACHED();
  return VideoDecoderConfig();
}

DemuxerStream::Type CoreAudioDemuxerStream::type() const {
  return AUDIO;
}

void CoreAudioDemuxerStream::EnableBitstreamConverter() {}

bool CoreAudioDemuxerStream::SupportsConfigChanges() { return false; }

void CoreAudioDemuxerStream::Stop() {
  if (!read_cb_.is_null()) {
    base::ResetAndReturn(&read_cb_)
        .Run(DemuxerStream::kOk, DecoderBuffer::CreateEOSBuffer());
  }

  demuxer_->ResetDataSourceOffset();
}

void CoreAudioDemuxerStream::Abort() {
  if (!read_cb_.is_null()) {
    base::ResetAndReturn(&read_cb_).Run(kAborted, NULL);
  }
}

bool CoreAudioDemuxerStream::Seek(base::TimeDelta time) {
  // Timestamps calcuations and seek is done mainly by Pipeline::SeekTask and
  // Pipeline::DoSeek.
  // Offset is calculated from begining of data source stream, not current
  // position. If data_source_offset is not reset, then demuxer will be moved
  // to (current position + seek time) instead of (seek time).
  demuxer_->ResetDataSourceOffset();
  pending_seek_ = true;
  return true;
}

OSStatus CoreAudioDemuxerStream::EnqueueBuffer() {
  OSStatus err = noErr;
  AudioQueueBufferRef fill_buf = audio_queue_buffer_;
  fill_buf->mAudioDataByteSize = bytes_filled_;

  if (!is_enqueue_running_) {
    is_enqueue_running_ = true;
    // |output_format_| should be something similar to |input_format_|.
    FillOutASBDForLPCM(output_format_,
                       input_format_.mSampleRate,
                       input_format_.mChannelsPerFrame,
                       16,
                       16,
                       false,
                       false,
                       false);

    AudioChannelLayout acl = {0};
    acl.mChannelLayoutTag = kAudioChannelLayoutTag_Stereo;
    if (input_format_.mChannelsPerFrame == 1)
      acl.mChannelLayoutTag = kAudioChannelLayoutTag_Mono;
    acl.mNumberChannelDescriptions = 0;
    acl.mChannelBitmap = 0;

    err = AudioQueueSetOfflineRenderFormat(audio_queue_, &output_format_, &acl);
    if (err) {
      LOG(ERROR) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
                 << " AudioQueueSetOfflineRenderFormat, error = "
                 << StringFromAudioError(err);
      return err;
    }

    // Value '16' in below calculation should be bits/sample, but very often
    // this value is 0 in input_format_.mBitsPerChannel.
    if (bit_rate_) {
      float ratio =
          (input_format_.mSampleRate * input_format_.mChannelsPerFrame * 16) /
          bit_rate_;
      decoded_data_buffer_size_ =
          CoreAudioDemuxer::kStreamInfoBufferSize * (ratio + 1);
    }
    if (!bit_rate_ || !decoded_data_buffer_size_) {
      // according to ISO bit rate must be at least 32kbps, which leads us for
      // compression ratio of 44.1. Buffer of this size should be enough for
      // any compressed audio data.
      decoded_data_buffer_size_ = 45 * CoreAudioDemuxer::kStreamInfoBufferSize;
    }

    err = AudioQueueAllocateBuffer(
        audio_queue_, decoded_data_buffer_size_, &output_buffer_);
    if (err) {
      LOG(ERROR) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
                 << " AudioQueueAllocateBuffer, error = "
                 << StringFromAudioError(err);
      return err;
    }

    err = AudioQueueStart(audio_queue_, NULL);
    if (err) {
      LOG(ERROR) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
                 << " AudioQueueStart, error = " << StringFromAudioError(err);
      return err;
    }

    // This is requirement!
    err =
        AudioQueueOfflineRender(audio_queue_, &time_stamp_, output_buffer_, 0);
    if (err) {
      LOG(ERROR) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
                 << " AudioQueueOfflineRender, error = "
                 << StringFromAudioError(err);
      return err;
    }
  }

  err = AudioQueueEnqueueBuffer(
        audio_queue_, fill_buf, packets_filled_, packet_descs_->data());
  if (err) {
    LOG(ERROR) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
               << " AudioQueueEnqueueBuffer, error = "
               << StringFromAudioError(err);
    return err;
  }

  UInt32 req_frames = decoded_data_buffer_size_ / output_format_.mBytesPerFrame;

  err = AudioQueueOfflineRender(
      audio_queue_, &time_stamp_, output_buffer_, req_frames);

  if (err) {
    LOG(ERROR) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
               << " AudioQueueOfflineRender, error = "
               << StringFromAudioError(err);
    return err;
  }

  frames_decoded_ = output_buffer_->mAudioDataByteSize;
  bytes_filled_ = 0;
  packets_filled_ = 0;

  return err;
}

void CoreAudioDemuxerStream::AudioPropertyListenerProc(
    void* client_data,
    AudioFileStreamID audio_file_stream,
    AudioFileStreamPropertyID property_id,
    UInt32* io_flags) {
  if (property_id == kAudioFileStreamProperty_ReadyToProducePackets) {
    CoreAudioDemuxerStream* stream =
        reinterpret_cast<CoreAudioDemuxerStream*>(client_data);
    stream->reading_audio_data_ = true;
  }
}

void CoreAudioDemuxerStream::AudioPacketsProc(
    void* client_data,
    UInt32 number_bytes,
    UInt32 number_packets,
    const void* input_data,
    AudioStreamPacketDescription* packet_descriptions) {
  CoreAudioDemuxerStream* stream =
      reinterpret_cast<CoreAudioDemuxerStream*>(client_data);
  if (!stream->audio_queue_)
    return;

  // the following code assumes we're streaming VBR data.
  for (UInt32 i = 0; i < number_packets; ++i) {
    SInt64 packet_offset = packet_descriptions[i].mStartOffset;
    SInt64 packet_size = packet_descriptions[i].mDataByteSize;

    // copy data to the audio queue buffer
    AudioQueueBufferRef fill_buf = stream->audio_queue_buffer_;
    memcpy(
        reinterpret_cast<char*>(fill_buf->mAudioData) + stream->bytes_filled_,
        (const char*)input_data + packet_offset,
        packet_size);

    // fill out packet description
    stream->packet_descs_->data()[stream->packets_filled_] =
        packet_descriptions[i];
    stream->packet_descs_->data()[stream->packets_filled_].mStartOffset =
        stream->bytes_filled_;
    stream->bytes_filled_ += packet_size;
    stream->packets_filled_++;
  }
}

void CoreAudioDemuxerStream::AudioQueueOutputCallback(
    void* client_data,
    AudioQueueRef audio_queue,
    AudioQueueBufferRef buffer) {
  // This is called by the audio queue when it has finished decoding our data.
  // The buffer is now free to be reused. Don't need to do anything here, since
  // overlapping reads are not supported. All data should be decoded before
  // new data from Read are available, and those data are not needed anymore.
}

}  // namespace media
