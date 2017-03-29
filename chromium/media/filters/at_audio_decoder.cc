// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (C) 2015 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA.
//
#include "media/filters/at_audio_decoder.h"

#include <AudioToolbox/AudioFileStream.h>
#include <AudioToolbox/AudioFormat.h>

#include <deque>

#include "base/bind.h"
#include "base/location.h"
#include "base/mac/mac_logging.h"
#include "base/single_thread_task_runner.h"
#include "media/base/audio_buffer.h"
#include "media/base/mac/framework_type_conversions.h"
#include "media/formats/mpeg/adts_constants.h"

namespace media {

namespace {

// Custom error codes returned from ProvideData() and passed on to the caller
// of AudioConverterFillComplexBuffer().
const OSStatus kDataConsumed = 'CNSM';   // No more input data currently.
const OSStatus kInvalidArgs = 'IVLD';    // Unexpected callback arguments.

struct ScopedAudioFileStreamIDTraits {
  static void Retain(AudioFileStreamID /* stream_id */) {
    NOTREACHED() << "Only compatible with ASSUME policy";
  }
  static void Release(AudioFileStreamID stream_id) {
    AudioFileStreamClose(stream_id);
  }
};

using ScopedAudioFileStreamID =
    base::ScopedTypeRef<AudioFileStreamID, ScopedAudioFileStreamIDTraits>;

// Wraps an input buffer and some metadata.  Used as the type of the user data
// passed between the caller of AudioConverterFillComplexBuffer() and the
// ProvideData() callback.
struct InputData {
  // Strip the ADTS header from the buffer.  Required for AudioConverter to
  // accept the input data.
  InputData(const DecoderBuffer& buffer, int channel_count)
      : data(buffer.data() + kADTSHeaderMinSize),
        data_size(buffer.data_size() - kADTSHeaderMinSize),
        channel_count(channel_count),
        packet_description({0}),
        consumed(false) {
    DCHECK_GE(buffer.data_size(), kADTSHeaderMinSize)
        << "We assume the input buffers contain ADTS headers";
    packet_description.mDataByteSize = data_size;
  }

  const void* data;
  size_t data_size;
  int channel_count;
  AudioStreamPacketDescription packet_description;
  bool consumed;
};

// Used as the data-supply callback for AudioConverterFillComplexBuffer().
OSStatus ProvideData(AudioConverterRef inAudioConverter,
                     UInt32* ioNumberDataPackets,
                     AudioBufferList* ioData,
                     AudioStreamPacketDescription** outDataPacketDescription,
                     void* inUserData) {
  DVLOG(5) << "AudioConverter wants " << *ioNumberDataPackets
           << " input frames";

  InputData* const input_data = reinterpret_cast<InputData*>(inUserData);
  DCHECK(input_data);
  if (input_data->consumed) {
    DVLOG(5) << "But there is no more input data";
    *ioNumberDataPackets = 0;
    return kDataConsumed;
  }

  if (ioData->mNumberBuffers != 1u) {
    DVLOG(1) << "Expected 1 output buffer, got " << ioData->mNumberBuffers;
    return kInvalidArgs;
  }

  DVLOG(5) << "Providing " << input_data->data_size << " bytes";

  ioData->mBuffers[0].mNumberChannels = input_data->channel_count;
  ioData->mBuffers[0].mDataByteSize = input_data->data_size;
  ioData->mBuffers[0].mData = const_cast<void*>(input_data->data);

  if (outDataPacketDescription)
    *outDataPacketDescription = &input_data->packet_description;

  input_data->consumed = true;
  return noErr;
}

std::string FourCCToString(uint32_t fourcc) {
  char buffer[4];
  buffer[0] = (fourcc >> 24) & 0xff;
  buffer[1] = (fourcc >> 16) & 0xff;
  buffer[2] = (fourcc >> 8) & 0xff;
  buffer[3] = fourcc & 0xff;
  return std::string(buffer, arraysize(buffer));
}

// Fills out the output format to meet Chrome pipeline requirements.
void GetOutputFormat(const AudioStreamBasicDescription& input_format,
                     AudioStreamBasicDescription* output_format) {
  DVLOG(1) << __func__;

  memset(output_format, 0, sizeof(*output_format));
  output_format->mFormatID = kAudioFormatLinearPCM;
  output_format->mFormatFlags = kLinearPCMFormatFlagIsFloat;
  output_format->mSampleRate = input_format.mSampleRate;
  output_format->mChannelsPerFrame = input_format.mChannelsPerFrame;
  output_format->mBitsPerChannel = 32;
  output_format->mBytesPerFrame =
      output_format->mChannelsPerFrame * output_format->mBitsPerChannel / 8;
  output_format->mFramesPerPacket = 1;
  output_format->mBytesPerPacket = output_format->mBytesPerFrame;
}

}  // namespace

// A helper class for reading audio format information from a sequence of audio
// buffers by feeding them into an AudioFileStream.
class ATAudioDecoder::AudioFormatReader {
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

  bool has_queued_buffers() const { return !buffers_.empty(); }
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

bool ATAudioDecoder::AudioFormatReader::ParseAndQueueBuffer(
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
ATAudioDecoder::AudioFormatReader::ReclaimQueuedBuffer() {
  DVLOG(1) << __func__;
  DCHECK(!buffers_.empty());

  auto result = buffers_.front();
  buffers_.pop_front();
  return result;
}

// static
void ATAudioDecoder::AudioFormatReader::OnAudioFileStreamProperty(
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
void ATAudioDecoder::AudioFormatReader::OnAudioFileStreamData(
    void* inClientData,
    UInt32 inNumberBytes,
    UInt32 inNumberPackets,
    const void* inInputData,
    AudioStreamPacketDescription* inPacketDescriptions) {
  DVLOG(1) << __func__ << ", ignoring";
}

bool ATAudioDecoder::AudioFormatReader::ReadFormatList() {
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

  scoped_ptr<AudioFormatListItem[]> format_list(
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

// static
void ATAudioDecoder::ScopedAudioConverterRefTraits::Retain(
    AudioConverterRef /* converter */) {
  NOTREACHED() << "Only compatible with ASSUME policy";
}

// static
void ATAudioDecoder::ScopedAudioConverterRefTraits::Release(
    AudioConverterRef converter) {
  const OSStatus status = AudioConverterDispose(converter);
  if (status != noErr)
    OSSTATUS_DVLOG(1, status) << FourCCToString(status)
                              << ": Failed to dispose of AudioConverter";
}

ATAudioDecoder::ATAudioDecoder(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner)
    : task_runner_(task_runner) {
}

ATAudioDecoder::~ATAudioDecoder() = default;

std::string ATAudioDecoder::GetDisplayName() const {
  return "ATAudioDecoder";
}

void ATAudioDecoder::Initialize(const AudioDecoderConfig& config,
                                const InitCB& init_cb,
                                const OutputCB& output_cb) {
  DVLOG(1) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(config.IsValidConfig());

  if (config.codec() != kCodecAAC) {
    DVLOG(1) << "Codec is " << config.codec() << ", but we only support AAC ("
             << kCodecAAC << ")";
    task_runner_->PostTask(FROM_HERE,
                           base::Bind(init_cb, false));
    return;
  }

  if (config.codec_delay() > 0) {
    DVLOG(1) << "Can't handle codec delay yet";
    task_runner_->PostTask(FROM_HERE,
                           base::Bind(init_cb, false));
    return;
  }

  if (!ReadInputChannelLayoutFromEsds(config)) {
    task_runner_->PostTask(FROM_HERE,
                           base::Bind(init_cb, false));
    return;
  }

  // This decoder supports re-initialization.
  converter_.reset();

  config_ = config;
  output_cb_ = output_cb;

  // Tell the pipeline this decoder is ready so that we start receiving input
  // samples via Decode().  To initialize |converter_|, we need to parse a bit
  // of the audio stream to let AutioToolbox figure out the audio format
  // specifics from the magic cookie, etc.  See InitializeConverter().

  task_runner_->PostTask(FROM_HERE, base::Bind(init_cb, true));
}

void ATAudioDecoder::Decode(const scoped_refptr<DecoderBuffer>& buffer,
                            const DecodeCB& decode_cb) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  Status status = kOk;

  if (!buffer->end_of_stream()) {
    if (!converter_) {
      if (!MaybeInitializeConverter(buffer))
        status = kDecodeError;
    } else {
      // Will call the OutputCB as appropriate.
      if (!ConvertAudio(buffer))
        status = kDecodeError;
    }
  }

  task_runner_->PostTask(FROM_HERE, base::Bind(decode_cb, status));
}

void ATAudioDecoder::Reset(const base::Closure& closure) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  // There is no |converter_| if Reset() is called before Decode(), which is
  // legal.
  if (converter_) {
    const OSStatus status = AudioConverterReset(converter_);
    if (status != noErr)
      OSSTATUS_DVLOG(1, status) << FourCCToString(status)
                                << ": Failed to reset AudioConverter";
  }

  task_runner_->PostTask(FROM_HERE, closure);
}

bool ATAudioDecoder::ReadInputChannelLayoutFromEsds(
    const AudioDecoderConfig& config) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  UInt32 channel_layout_size = 0;
  OSStatus status = AudioFormatGetPropertyInfo(
      kAudioFormatProperty_ChannelLayoutFromESDS, config.extra_data_size(),
      config.extra_data(), &channel_layout_size);
  if (status != noErr) {
    OSSTATUS_DVLOG(1, status) << FourCCToString(status)
                              << ": Failed to get channel layout info";
    return false;
  }

  input_channel_layout_.reset(
      static_cast<AudioChannelLayout*>(malloc(channel_layout_size)));
  status = AudioFormatGetProperty(
      kAudioFormatProperty_ChannelLayoutFromESDS, config.extra_data_size(),
      config.extra_data(), &channel_layout_size, input_channel_layout_.get());
  if (status != noErr) {
    OSSTATUS_DVLOG(1, status) << FourCCToString(status)
                              << ": Failed to get channel layout";
    return false;
  }

  return true;
}

bool ATAudioDecoder::MaybeInitializeConverter(
    const scoped_refptr<DecoderBuffer>& buffer) {
  DVLOG(1) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (!input_format_reader_)
    input_format_reader_.reset(new AudioFormatReader);

  if (!input_format_reader_->ParseAndQueueBuffer(buffer))
    return false;

  if (!input_format_reader_->is_finished())
    // Must parse more audio stream bytes.  Try again with the next call to
    // Decode().
    return true;

  AudioStreamBasicDescription input_format =
      input_format_reader_->audio_format();

  AudioStreamBasicDescription output_format;
  GetOutputFormat(input_format, &output_format);

  OSStatus status = AudioConverterNew(&input_format, &output_format,
                                      converter_.InitializeInto());
  if (status != noErr) {
    OSSTATUS_DVLOG(1, status) << FourCCToString(status)
                              << ": Failed to create AudioConverter";
    return false;
  }

  status = AudioConverterSetProperty(
      converter_, kAudioConverterInputChannelLayout,
      sizeof(*input_channel_layout_), input_channel_layout_.get());
  if (status != noErr) {
    OSSTATUS_DVLOG(1, status) << FourCCToString(status)
                              << ": Failed to set input channel layout";
    return false;
  }

  AudioChannelLayout output_channel_layout = {0};
  output_channel_layout.mChannelLayoutTag =
      ChromeChannelLayoutToCoreAudioTag(config_.channel_layout());
  status = AudioConverterSetProperty(
      converter_, kAudioConverterOutputChannelLayout,
      sizeof(output_channel_layout), &output_channel_layout);
  if (status != noErr) {
    OSSTATUS_DVLOG(1, status) << FourCCToString(status)
                              << ": Failed to set output channel layout";
    return false;
  }

  // Consume any input buffers queued in |input_format_reader_|.
  while (input_format_reader_->has_queued_buffers()) {
    const auto& queued_buffer = input_format_reader_->ReclaimQueuedBuffer();
    // Will call the OutputCB as appropriate.
    if (!ConvertAudio(queued_buffer))
      return false;
  }

  input_format_reader_.reset();
  return true;
}

bool ATAudioDecoder::ConvertAudio(const scoped_refptr<DecoderBuffer>& input) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(converter_);

  const SampleFormat kOutputSampleFormat = kSampleFormatF32;
  // The actual frame count is supposed to be 1024, or 960 in rare cases.
  // Prepare for twice as much to allow for SBR: With Spectral Band
  // Replication, the output sampling rate is twice the input sapmling rate,
  // leading to twice as much output data.
  const uint32_t kMaxOutputFrameCount = kSamplesPerAACFrame * 2;

  UInt32 output_frame_count = kMaxOutputFrameCount;

  // Pre-allocate a buffer for the maximum expected frame count.  We will let
  // the AudioConverter fill it with decoded audio, through |output_buffers|
  // defined below.
  const scoped_refptr<AudioBuffer> output = AudioBuffer::CreateBuffer(
      kOutputSampleFormat, config_.channel_layout(),
      ChannelLayoutToChannelCount(config_.channel_layout()),
      config_.samples_per_second(), output_frame_count);

  InputData input_data(*input, output->channel_count());

  AudioBufferList output_buffers;
  output_buffers.mNumberBuffers = 1;
  output_buffers.mBuffers[0].mNumberChannels = output->channel_count();
  output_buffers.mBuffers[0].mDataByteSize =
      output->frame_count() * output->channel_count() *
      SampleFormatToBytesPerChannel(kOutputSampleFormat);
  // Will put decoded data in the |output| media::AudioBuffer directly.
  output_buffers.mBuffers[0].mData = output->channel_data()[0];

  AudioStreamPacketDescription output_packet_descriptions[kMaxOutputFrameCount];

  const OSStatus status = AudioConverterFillComplexBuffer(
      converter_, &ProvideData, &input_data, &output_frame_count,
      &output_buffers, output_packet_descriptions);

  if (status != noErr && status != kDataConsumed) {
    OSSTATUS_DVLOG(1, status) << FourCCToString(status)
                              << ": Failed to convert audio";
    return false;
  }
  DCHECK(input_data.consumed);

  DVLOG(5) << "Decoded " << output_frame_count << " frames @"
           << input->timestamp();

  if (output_frame_count > kMaxOutputFrameCount) {
    DVLOG(1) << "Unexpected output sample count: " << output_frame_count;
    return false;
  }

  if (output_frame_count > 0) {
    output->TrimEnd(kMaxOutputFrameCount - output_frame_count);
    output->set_timestamp(input->timestamp());
    task_runner_->PostTask(FROM_HERE, base::Bind(output_cb_, output));
  }

  return true;
}

}  // namespace media
