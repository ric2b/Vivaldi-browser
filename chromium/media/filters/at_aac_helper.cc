// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (C) 2015 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA.

#include "media/filters/at_aac_helper.h"

#include <AudioToolbox/AudioFileStream.h>
#include <AudioToolbox/AudioFormat.h>

#include <algorithm>
#include <deque>

#include "base/mac/mac_logging.h"
#include "base/mac/scoped_typeref.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/decoder_buffer.h"
#include "media/base/mac/framework_type_conversions.h"
#include "media/formats/mpeg/adts_constants.h"

namespace media {

namespace {

using ScopedAudioChannelLayoutPtr = ATCodecHelper::ScopedAudioChannelLayoutPtr;

ScopedAudioChannelLayoutPtr GetInputChannelLayoutFromChromeChannelLayout(
    const AudioDecoderConfig& config) {
  ScopedAudioChannelLayoutPtr layout(
      static_cast<AudioChannelLayout*>(malloc(sizeof(AudioChannelLayout))));
  memset(layout.get(), 0, sizeof(AudioChannelLayout));
  layout->mChannelLayoutTag =
      ChromeChannelLayoutToCoreAudioTag(config.channel_layout());

  return layout;
}

ScopedAudioChannelLayoutPtr ReadInputChannelLayoutFromEsds(
    const AudioDecoderConfig& config) {
  UInt32 channel_layout_size = 0;
  OSStatus status = AudioFormatGetPropertyInfo(
      kAudioFormatProperty_ChannelLayoutFromESDS, config.extra_data().size(),
      &config.extra_data()[0], &channel_layout_size);
  if (status != noErr) {
    OSSTATUS_DVLOG(1, status) << FourCCToString(status)
                              << ": Failed to get channel layout info";
    return nullptr;
  }

  ScopedAudioChannelLayoutPtr layout(
      static_cast<AudioChannelLayout*>(malloc(channel_layout_size)));
  status = AudioFormatGetProperty(kAudioFormatProperty_ChannelLayoutFromESDS,
                                  config.extra_data().size(),
                                  &config.extra_data()[0],
                                  &channel_layout_size, layout.get());
  if (status != noErr) {
    OSSTATUS_DVLOG(1, status) << FourCCToString(status)
                              << ": Failed to get channel layout";
    return nullptr;
  }

  return layout;
}

struct ScopedAudioFileStreamIDTraits {
  static AudioFileStreamID Retain(AudioFileStreamID  stream_id) {
    NOTREACHED() << "Only compatible with ASSUME policy";
    return stream_id;
  }
  static void Release(AudioFileStreamID stream_id) {
    AudioFileStreamClose(stream_id);
  }
  static AudioFileStreamID InvalidValue() { return nullptr; }
};

using ScopedAudioFileStreamID =
    base::ScopedTypeRef<AudioFileStreamID, ScopedAudioFileStreamIDTraits>;

}  // namespace

// A helper class for reading audio format information from a sequence of audio
// buffers by feeding them into an AudioFileStream.
class ATAACHelper::AudioFormatReader {
 public:
  AudioFormatReader() { memset(&format_, 0, sizeof(format_)); }

  // Feeds data from |buffer| into |stream_| in order to let AudioToolbox
  // determine the input format for us.  The input format arrives via the
  // property-listener OnAudioFileStreamProperty().
  bool ParseAndQueueBuffer(const scoped_refptr<DecoderBuffer>& buffer);

  bool is_finished() const { return format_.mFormatID != 0; }

  AudioStreamBasicDescription audio_format() const {
    DCHECK(is_finished());
    return format_;
  }

  scoped_refptr<DecoderBuffer> ReclaimQueuedBuffer();

 private:
  // Used as the property-listener callback for AudioFileStreamOpen().  Upon
  // encountering the format list property, picks the most appropriate format
  // and stores it in |format_|.
  static void OnAudioFileStreamProperty(void* inClientData,
                                        AudioFileStreamID inAudioFileStream,
                                        AudioFileStreamPropertyID inPropertyID,
                                        UInt32* ioFlags);
  // Used as the audio-data callback for AudioFileStreamOpen().
  static void OnAudioFileStreamData(
      void* inClientData,
      UInt32 inNumberBytes,
      UInt32 inNumberPackets,
      const void* inInputData,
      AudioStreamPacketDescription* inPacketDescriptions);

  bool ReadFormatList();
  bool error() const { return !stream_; }

  ScopedAudioFileStreamID stream_;
  AudioStreamBasicDescription format_;
  std::deque<scoped_refptr<DecoderBuffer>> buffers_;

  DISALLOW_COPY_AND_ASSIGN(AudioFormatReader);
};

bool ATAACHelper::AudioFormatReader::ParseAndQueueBuffer(
    const scoped_refptr<DecoderBuffer>& buffer) {
  DVLOG(1) << __func__;

  buffers_.push_back(buffer);

  if (!stream_) {
    const OSStatus status = AudioFileStreamOpen(
        this, &OnAudioFileStreamProperty, &OnAudioFileStreamData,
        kAudioFileAAC_ADTSType, stream_.InitializeInto());
    if (status != noErr) {
      OSSTATUS_DVLOG(1, status) << FourCCToString(status)
                                << ": Failed to open audio file stream";
      return false;
    }
  }

  DCHECK(stream_);
  const OSStatus status = AudioFileStreamParseBytes(
      stream_, buffer->data_size(), buffer->data(), 0);
  if (status != noErr) {
    OSSTATUS_DVLOG(1, status) << FourCCToString(status)
                              << ": Failed to parse audio file stream";
    return false;
  }

  return !error();
}

scoped_refptr<DecoderBuffer>
ATAACHelper::AudioFormatReader::ReclaimQueuedBuffer() {
  DVLOG(1) << __func__;

  if (buffers_.empty())
    return nullptr;

  auto result = buffers_.front();
  buffers_.pop_front();
  return result;
}

// static
void ATAACHelper::AudioFormatReader::OnAudioFileStreamProperty(
    void* inClientData,
    AudioFileStreamID inAudioFileStream,
    AudioFileStreamPropertyID inPropertyID,
    UInt32* ioFlags) {
  DVLOG(1) << __func__ << "(" << FourCCToString(inPropertyID) << ")";

  if (inPropertyID != kAudioFileStreamProperty_FormatList)
    return;

  auto* format_reader = static_cast<AudioFormatReader*>(inClientData);
  DCHECK_EQ(inAudioFileStream, format_reader->stream_.get());

  if (!format_reader->ReadFormatList())
    format_reader->stream_.reset();
}

// static
void ATAACHelper::AudioFormatReader::OnAudioFileStreamData(
    void* inClientData,
    UInt32 inNumberBytes,
    UInt32 inNumberPackets,
    const void* inInputData,
    AudioStreamPacketDescription* inPacketDescriptions) {
  DVLOG(1) << __func__ << ", ignoring";
}

bool ATAACHelper::AudioFormatReader::ReadFormatList() {
  DVLOG(1) << __func__;

  UInt32 format_list_size = 0;
  OSStatus status = AudioFileStreamGetPropertyInfo(
      stream_, kAudioFileStreamProperty_FormatList, &format_list_size, nullptr);
  if (status != noErr || format_list_size % sizeof(AudioFormatListItem) != 0) {
    OSSTATUS_DVLOG(1, status) << FourCCToString(status)
                              << ": Failed to get format list count";
    return false;
  }

  const size_t format_count = format_list_size / sizeof(AudioFormatListItem);
  DVLOG(1) << "Found " << format_count << " formats";

  std::unique_ptr<AudioFormatListItem[]> format_list(
      new AudioFormatListItem[format_count]);
  status =
      AudioFileStreamGetProperty(stream_, kAudioFileStreamProperty_FormatList,
                                 &format_list_size, format_list.get());
  if (status != noErr ||
      format_list_size != format_count * sizeof(AudioFormatListItem)) {
    OSSTATUS_DVLOG(1, status) << FourCCToString(status)
                              << ": Failed to get format list";
    return false;
  }

  UInt32 format_index = 0;
  UInt32 format_index_size = sizeof(format_index);
  status = AudioFormatGetProperty(
      kAudioFormatProperty_FirstPlayableFormatFromList, format_list_size,
      format_list.get(), &format_index_size, &format_index);
  if (status != noErr) {
    OSSTATUS_DVLOG(1, status) << FourCCToString(status)
                              << ": Failed to get format from list";
    return false;
  }

  format_ = format_list[format_index].mASBD;

  if (format_.mFormatID != 0) {
    DVLOG(1) << "mSampleRate = " << format_.mSampleRate;
    DVLOG(1) << "mFormatID = " << FourCCToString(format_.mFormatID);
    DVLOG(1) << "mFormatFlags = " << format_.mFormatFlags;
    DVLOG(1) << "mChannelsPerFrame = " << format_.mChannelsPerFrame;
  }

  return true;
}

ATAACHelper::ATAACHelper() : input_format_reader_(new AudioFormatReader) {}

ATAACHelper::~ATAACHelper() = default;

bool ATAACHelper::Initialize(const AudioDecoderConfig& config,
                             const InputFormatKnownCB& input_format_known_cb,
                             const ConvertAudioCB& convert_audio_cb) {
  DCHECK_EQ(0, config.codec_delay());

  input_format_known_cb_ = input_format_known_cb;
  convert_audio_cb_ = convert_audio_cb;

  // Prefer to let Audio Toolbox figure out the channel layout from the ESDS
  // itself.  Fall back to the layout specified by AudioDecoderConfig.
  input_channel_layout_ = ReadInputChannelLayoutFromEsds(config);
  if (!input_channel_layout_) {
    input_channel_layout_ =
        GetInputChannelLayoutFromChromeChannelLayout(config);
  }

  // We are not fully initialized yet, because the input format is still not
  // known.  We will figure it out from the audio stream itself in
  // ProcessBuffer() and only then invoke |input_format_known_cb_|.

  return true;
}

bool ATAACHelper::ProcessBuffer(const scoped_refptr<DecoderBuffer>& buffer) {
  if (!is_input_format_known())
    return !buffer->end_of_stream() ? ReadInputFormat(buffer) : true;

  return ConvertAudio(buffer);
}

bool ATAACHelper::ReadInputFormat(const scoped_refptr<DecoderBuffer>& buffer) {
  if (!input_format_reader_->ParseAndQueueBuffer(buffer))
    return false;

  if (!input_format_reader_->is_finished())
    // Must parse more audio stream bytes.  Try again with the next call to
    // ProcessBuffer().
    return true;

  if (!input_format_known_cb_.Run(input_format_reader_->audio_format(),
                                  std::move(input_channel_layout_)))
    return false;

  // Consume any input buffers queued in |input_format_reader_|.
  while (const scoped_refptr<DecoderBuffer> queued_buffer =
             input_format_reader_->ReclaimQueuedBuffer()) {
    if (!ConvertAudio(queued_buffer))
      return false;
  }

  input_format_reader_.reset();
  return true;
}

bool ATAACHelper::ConvertAudio(const scoped_refptr<DecoderBuffer>& buffer) {
  // The actual frame count is supposed to be 1024, or 960 in rare cases.
  // Prepare for twice as much to allow for SBR: With Spectral Band
  // Replication, the output sampling rate is twice the input sapmling rate,
  // leading to twice as much output data.
  const size_t kMaxOutputFrameCount = kSamplesPerAACFrame * 2;

  return convert_audio_cb_.Run(buffer, kADTSHeaderMinSize,
                               kMaxOutputFrameCount);
}

}  // namespace media
