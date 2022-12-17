// -*- Mmtde: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2015 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA.

#include "platform_media/renderer/decoders/mac/at_audio_decoder.h"

#include <AudioToolbox/AudioToolbox.h>
#include <algorithm>

#include "base/bind.h"
#include "base/location.h"
#include "base/mac/mac_logging.h"
#include "base/mac/mac_util.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "media/base/audio_buffer.h"
#include "media/base/audio_discard_helper.h"
#include "media/base/decoder_status.h"
#include "media/base/demuxer_stream.h"
#include "media/formats/mpeg/adts_constants.h"
#include "platform_media/common/mac/framework_type_conversions.h"
#include "platform_media/common/platform_logging_util.h"

namespace media {

struct ATAudioDecoder::FormatDetection {
  FormatDetection() = default;
  ~FormatDetection() { CloseStream(); }

  FormatDetection(const FormatDetection&) = delete;
  FormatDetection& operator=(const FormatDetection&) = delete;

  void CloseStream() {
    if (stream) {
      AudioFileStreamClose(stream);
      stream = nullptr;
    }
  }

  bool format_ready = false;
  AudioFileStreamID stream = nullptr;
  AudioStreamBasicDescription format{};
  std::vector<scoped_refptr<DecoderBuffer>> queue;
};

namespace {

// If true, the decoder can be used with FFmpeg Demuxer.
constexpr bool kAllowFfmpegDemuxer = true;

const SampleFormat kOutputSampleFormat = SampleFormat::kSampleFormatF32;

// Custom error codes returned from ProvideData() and passed on to the caller
// of AudioConverterFillComplexBuffer().
const OSStatus kDataConsumed = 'CNSM';  // No more input data currently.
const OSStatus kInvalidArgs = 'IVLD';   // Unexpected callback arguments.

using FormatDetection = ATAudioDecoder::FormatDetection;

// Wraps an input buffer and some metadata.  Used as the type of the user data
// passed between the caller of AudioConverterFillComplexBuffer() and the
// ProvideData() callback.
struct InputData {
  // Strip the ADTS header from the buffer.  Required for AudioConverter to
  // accept the input data.
  InputData(const DecoderBuffer& buffer, int channel_count, size_t header_size)
      : data(buffer.data() + header_size),
        data_size(buffer.data_size() - header_size),
        channel_count(channel_count) {
    DCHECK_GE((int)buffer.data_size(), base::checked_cast<int>(header_size));
    packet_description.mDataByteSize = data_size;
  }

  // Constructs an InputData object representing "no data".
  InputData() = default;

  const void* data = nullptr;
  size_t data_size = 0;
  int channel_count = 0;
  AudioStreamPacketDescription packet_description{};
  bool consumed = false;
};

void GetHeaderSizeAndMaxFrameCount(const AudioDecoderConfig& config,
                                   size_t& header_size,
                                   size_t& max_output_frame_count) {
  DCHECK_EQ(header_size, 0u);
  DCHECK_EQ(max_output_frame_count, 0u);
  switch (config.codec()) {
    case AudioCodec::kAAC:
      // FFmpegDemuxer already stripped the header, while
      // MultiBufferDataSource kept it.
      header_size =
          config.platform_media_ffmpeg_demuxer_ ? 0 : kADTSHeaderMinSize;

      // The actual frame count is supposed to be 1024, or 960 in rare cases.
      // Prepare for twice as much to allow for SBR: With Spectral Band
      // Replication, the output sampling rate is twice the input sapmling rate,
      // leading to twice as much output data.
      max_output_frame_count = kSamplesPerAACFrame * 2;
      break;
    default:
      NOTREACHED() << "Bad codec: " << GetCodecName(config.codec());
      break;
  }
}

// Used as the data-supply callback for AudioConverterFillComplexBuffer().
OSStatus ProvideData(AudioConverterRef inAudioConverter,
                     UInt32* ioNumberDataPackets,
                     AudioBufferList* ioData,
                     AudioStreamPacketDescription** outDataPacketDescription,
                     void* inUserData) {
  InputData* const input_data = static_cast<InputData*>(inUserData);
  DCHECK(input_data);

  if (input_data->consumed) {
    VLOG(5) << " PROPMEDIA(RENDERER) : " << __FUNCTION__ << " consumed";
    *ioNumberDataPackets = 0;
    return kDataConsumed;
  }

  if (ioData->mNumberBuffers != 1u) {
    VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
            << " Expected 1 output buffer, got " << ioData->mNumberBuffers;
    return kInvalidArgs;
  }

  VLOG(5) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
          << " input_frames=" << *ioNumberDataPackets
          << " data_size=" << input_data->data_size << "";

  ioData->mBuffers[0].mNumberChannels = input_data->channel_count;
  ioData->mBuffers[0].mDataByteSize = input_data->data_size;
  ioData->mBuffers[0].mData = const_cast<void*>(input_data->data);

  if (outDataPacketDescription)
    *outDataPacketDescription = &input_data->packet_description;

  input_data->consumed = true;
  return noErr;
}

// Fills out the output format to meet Chrome pipeline requirements.
void GetOutputFormat(const AudioStreamBasicDescription& input_format,
                     AudioStreamBasicDescription* output_format) {
  VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__;

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

// Adds |padding_frame_count| frames of silence to the front of |buffer| and
// returns the resulting buffer.  This is used when we need to "fix" the
// behavior of AudioConverter wrt codec delay handling.  If AudioConverter
// strips the codec delay internally, it's all fine unless we are decoding
// audio appended via MSE.  In this case, only the initial delay gets stripped,
// and the one after the append is not.  AudioDiscardHelper can do the
// stripping for us, using discard information from FrameProcessor, but then
// the codec delay must be present in the initial output buffer too, hence the
// padding that we're adding.
scoped_refptr<AudioBuffer> AddFrontPadding(
    const scoped_refptr<AudioBuffer>& buffer,
    size_t padding_frame_count) {
  VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__;

  const scoped_refptr<AudioBuffer> result = AudioBuffer::CreateBuffer(
      kOutputSampleFormat, buffer->channel_layout(), buffer->channel_count(),
      buffer->sample_rate(), padding_frame_count + buffer->frame_count());

  const size_t padding_size =
      padding_frame_count * buffer->channel_count() *
      SampleFormatToBytesPerChannel(kOutputSampleFormat);
  uint8_t* const result_data = result->channel_data()[0];
  std::fill(result_data, result_data + padding_size, 0);

  uint8_t* const buffer_data = buffer->channel_data()[0];
  const size_t buffer_size = buffer->frame_count() * buffer->channel_count() *
                             SampleFormatToBytesPerChannel(kOutputSampleFormat);
  std::copy(buffer_data, buffer_data + buffer_size, result_data + padding_size);

  return result;
}

bool GetInputChannelLayout(const AudioDecoderConfig& config,
                           const std::vector<uint8_t>& full_extra_data,
                           AudioChannelLayout& layout) {
  // Prefer to let Audio Toolbox figure out the channel layout from the ESDS
  // itself. Fall back to the layout specified by AudioDecoderConfig.
  if (!full_extra_data.empty()) {
    UInt32 size = sizeof(AudioChannelLayout);
    OSStatus status = AudioFormatGetProperty(
        kAudioFormatProperty_ChannelLayoutFromESDS, full_extra_data.size(),
        full_extra_data.data(), &size, &layout);
    if (status == noErr) {
      VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
              << " esds_layout : " << Loggable(layout.mChannelLayoutTag);
      return true;
    }
    OSSTATUS_VLOG(1, status)
        << " PROPMEDIA(RENDERER) : " << __FUNCTION__
        << ": Failed to get channel layout"
        << " Error Status : " << status << " size=" << size;
    layout = AudioChannelLayout{};
  }

  AudioChannelLayoutTag tag =
      ChromeChannelLayoutToCoreAudioTag(config.channel_layout());
  if (tag == kAudioChannelLayoutTag_Unknown) {
    LOG(WARNING) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
                 << " Failed to convert Chrome Channel Layout";
    return false;
  }

  layout.mChannelLayoutTag = tag;
  LOG(WARNING) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
               << " chrome_layout : " << Loggable(layout.mChannelLayoutTag);
  return true;
}

bool ReadFormatList(FormatDetection* detection) {
  VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__;

  UInt32 format_list_size = 0;
  OSStatus status = AudioFileStreamGetPropertyInfo(
      detection->stream, kAudioFileStreamProperty_FormatList, &format_list_size,
      nullptr);
  if (status != noErr || format_list_size % sizeof(AudioFormatListItem) != 0) {
    OSSTATUS_VLOG(1, status) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
                             << ": Failed to get format list count"
                             << " Error Status : " << status;
    return false;
  }

  const size_t format_count = format_list_size / sizeof(AudioFormatListItem);
  VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__ << " Found "
          << format_count << " formats";

  std::unique_ptr<AudioFormatListItem[]> format_list(
      new AudioFormatListItem[format_count]);
  status = AudioFileStreamGetProperty(detection->stream,
                                      kAudioFileStreamProperty_FormatList,
                                      &format_list_size, format_list.get());
  if (status != noErr ||
      format_list_size != format_count * sizeof(AudioFormatListItem)) {
    OSSTATUS_VLOG(1, status) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
                             << ": Failed to get format list"
                             << " Error Status : " << status;
    return false;
  }

  UInt32 format_index = 0;
  UInt32 format_index_size = sizeof(format_index);
  status = AudioFormatGetProperty(
      kAudioFormatProperty_FirstPlayableFormatFromList, format_list_size,
      format_list.get(), &format_index_size, &format_index);
  if (status != noErr) {
    OSSTATUS_VLOG(1, status) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
                             << ": Failed to get format from list"
                             << " Error Status : " << status;
    return false;
  }

  detection->format = format_list[format_index].mASBD;

  return true;
}

void OnAudioFileStreamProperty(void* inClientData,
                               AudioFileStreamID inAudioFileStream,
                               AudioFileStreamPropertyID inPropertyID,
                               UInt32* ioFlags) {
  VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__ << " ("
          << FourCCToString(inPropertyID) << ")";

  auto* detection = static_cast<FormatDetection*>(inClientData);
  DCHECK_EQ(inAudioFileStream, detection->stream);

  if (inPropertyID == kAudioFileStreamProperty_FormatList) {
    if (!ReadFormatList(detection)) {
      // Signal error.
      detection->CloseStream();
    }
  }
}

void OnAudioFileStreamData(void* inClientData,
                           UInt32 inNumberBytes,
                           UInt32 inNumberPackets,
                           const void* inInputData,
                           AudioStreamPacketDescription* inPacketDescriptions) {
  VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__ << ", ignoring";
}

bool RunFormatDetection(FormatDetection* detection,
                        scoped_refptr<DecoderBuffer> buffer) {
  DCHECK_EQ(detection->format_ready, false);
  if (buffer->end_of_stream()) {
    VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
            << ": eos while looking for input format";
    return true;
  }

  if (!detection->stream) {
    const OSStatus status = AudioFileStreamOpen(
        detection, &OnAudioFileStreamProperty, &OnAudioFileStreamData,
        kAudioFileAAC_ADTSType, &detection->stream);
    if (status != noErr) {
      OSSTATUS_VLOG(1, status) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
                               << ": Failed to open audio file stream"
                               << " Error Status : " << status;
      return false;
    }
  }

  DCHECK(detection->stream);
  const OSStatus status = AudioFileStreamParseBytes(
      detection->stream, buffer->data_size(), buffer->data(), 0);
  if (status != noErr) {
    OSSTATUS_VLOG(1, status) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
                             << ": Failed to parse audio file stream"
                             << " Error Status : " << status;
    return false;
  }

  if (!detection->stream) {
    // Error in ReadFormatList().
    return false;
  }

  detection->queue.push_back(std::move(buffer));

  const AudioStreamBasicDescription& format = detection->format;
  if (format.mFormatID == 0) {
    // Format is not yet known, continue scanning.
    return true;
  }

  detection->format_ready = true;

  VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
          << " mSampleRate = " << format.mSampleRate;
  VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
          << " mFormatID = " << FourCCToString(format.mFormatID);
  VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
          << " mFormatFlags = " << format.mFormatFlags;
  VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
          << " mChannelsPerFrame = " << format.mChannelsPerFrame;
  return true;
}

void PutAACDescriptior(std::vector<uint8_t>& buffer, int tag, unsigned size) {
  buffer.push_back(tag);
  for (int i = 3; i > 0; i--) {
    buffer.push_back((size >> (7 * i)) | 0x80);
  }
  buffer.push_back(size & 0x7F);
}

void PutZeros(std::vector<uint8_t>& buffer, int count) {
  for (; count > 0; --count) {
    buffer.push_back(0);
  }
}

// Find AudioToolbox format from the config filled by FFmpegDemuxer. Return
// patched extra_data to use for further configuration. This follows
// ffat_create_decoder in libavcodec/audiotoolboxdec.c in FFmpeg sources.
std::vector<uint8_t> FindInputFormatFromFFmpeg(
    const AudioDecoderConfig& config,
    AudioStreamBasicDescription& format) {
  DCHECK_EQ(config.codec(), AudioCodec::kAAC);
  DCHECK(config.platform_media_ffmpeg_demuxer_);

  format = AudioStreamBasicDescription{.mFormatID = kAudioFormatMPEG4AAC};
  std::vector<uint8_t> buffer;
  size_t nextra = config.extra_data().size();
  if (nextra != 0) {
    // FFmpegDemuxer leaves in extra_data() only few (like 2) ESDS bytes, but
    // AudioFormatGetProperty wants the whole header. So reconstruct it.
    constexpr size_t ndescr = 5;
    size_t header_size = ndescr + 3 + ndescr + 13 + ndescr + nextra;
    buffer.reserve(header_size);
    // ES descriptor
    PutAACDescriptior(buffer, 0x03, 3 + ndescr + 13 + ndescr + nextra);
    PutZeros(buffer, 2);
    buffer.push_back(0);  // flags (= no flags)

    // DecoderConfig descriptor
    PutAACDescriptior(buffer, 0x04, 13 + ndescr + nextra);
    buffer.push_back(0x40);  // Object type indication
    buffer.push_back(0x15);  // flags (= Audiostream)
    PutZeros(buffer, 3);     // Buffersize DB
    PutZeros(buffer, 4);     // maxbitrate
    PutZeros(buffer, 4);     // avgbitrate

    // DecoderSpecific info descriptor
    PutAACDescriptior(buffer, 0x05, nextra);
    buffer.insert(buffer.end(), config.extra_data().begin(),
                  config.extra_data().end());

    DCHECK_EQ(buffer.size(), header_size);

    UInt32 format_size = sizeof(AudioStreamBasicDescription);
    OSStatus status =
        AudioFormatGetProperty(kAudioFormatProperty_FormatInfo, buffer.size(),
                               buffer.data(), &format_size, &format);
    if (status == noErr && format_size == sizeof(AudioStreamBasicDescription))
      return buffer;

    OSSTATUS_VLOG(1, status) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
                             << ": Failed to get format_info"
                             << " status=" << status << " size=" << format_size;

    // Fallback to manual config.
    format = AudioStreamBasicDescription{.mFormatID = kAudioFormatMPEG4AAC};
  }

  // Deduce from FFmpeg config.
  format.mSampleRate = config.samples_per_second();
  format.mChannelsPerFrame = config.channels() ? config.channels() : 1;
  return buffer;
}

}  // namespace

ATAudioDecoder::ATAudioDecoder(
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : task_runner_(std::move(task_runner)) {}

ATAudioDecoder::~ATAudioDecoder() {
  CloseConverter();
}

AudioDecoderType ATAudioDecoder::GetDecoderType() const {
  return AudioDecoderType::kVivATAudio;
}

void ATAudioDecoder::Initialize(
    const AudioDecoderConfig& config,
    CdmContext* cdm_context,
    InitCB init_cb,
    const OutputCB& output_cb,
    const WaitingCB& waiting_for_decryption_key_cb) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(config.IsValidConfig());

  VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
          << " with AudioDecoderConfig :" << Loggable(config);

  if (config.is_encrypted()) {
    LOG(WARNING) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
                 << " Unsupported Encrypted Audio codec : "
                 << GetCodecName(config.codec());
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(init_cb),
                                  DecoderStatus::Codes::kUnsupportedCodec));
    return;
  }

  if (config.codec() != AudioCodec::kAAC) {
    VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
            << " Unsupported codec: " << GetCodecName(config.codec());
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(init_cb),
                                  DecoderStatus::Codes::kUnsupportedCodec));
    return;
  }

  // Chromium provides own code that uses MacOS API to play XHE_AAC audio that
  // FFmpeg does not support. Rely on it, see
  // chromium/media/filters/mac/audio_toolbox_audio_decoder.h
  if (config.profile() == AudioCodecProfile::kXHE_AAC) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(init_cb),
                       DecoderStatus::Codes::kUnsupportedProfile));
    return;
  }

  if (!kAllowFfmpegDemuxer && config.platform_media_ffmpeg_demuxer_) {
    VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
            << " ffmpeg demuxer is not supported";
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(init_cb),
                                  DecoderStatus::Codes::kUnsupportedCodec));
    return;
  }

  // This decoder supports re-initialization.
  CloseConverter();

  config_ = config;
  output_cb_ = output_cb;

  ResetTimestampState();

  debug_buffer_logger_.Initialize(GetCodecName(config_.codec()));

  if (!config_.platform_media_ffmpeg_demuxer_) {
    format_detection_ = std::make_unique<FormatDetection>();
  } else {
    // FFmpeg strips ADTS Header from packets so AudioFileStreamParseBytes does
    // not work to detect it automatically. Deduce the format from the config
    // instead.
    AudioStreamBasicDescription format;
    std::vector<uint8_t> full_extra_data =
        FindInputFormatFromFFmpeg(config_, format);
    if (!InitializeConverter(format, full_extra_data)) {
      task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(init_cb), DecoderStatus::Codes::kFailed));
      return;
    }
  }

  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(init_cb), DecoderStatus::Codes::kOk));
}

void ATAudioDecoder::Decode(scoped_refptr<DecoderBuffer> input,
                            DecodeCB decode_cb) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  VLOG(4) << " PROPMEDIA(RENDERER) : " << __FUNCTION__ << " input_size="
          << (input->end_of_stream() ? 0 : input->data_size());

  debug_buffer_logger_.Log(*input);

  bool ok;
  if (format_detection_) {
    ok = DetectFormat(std::move(input));
  } else {
    ok = ConvertAudio(std::move(input));
  }

  VLOG(5) << " PROPMEDIA(RENDERER) : " << __FUNCTION__ << " ok=" << ok;

  DecoderStatus status =
      ok ? DecoderStatus::Codes::kOk : DecoderStatus::Codes::kFailed;
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(std::move(decode_cb), status));
}

void ATAudioDecoder::Reset(base::OnceClosure closure) {
  VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__;
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  // There is no |converter_| if Reset() is called before Decode(), which is
  // legal.
  if (converter_) {
    const OSStatus status = AudioConverterReset(converter_);
    if (status != noErr)
      OSSTATUS_VLOG(1, status) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
                               << ": Failed to reset AudioConverter"
                               << " Error Status : " << status;
  }

  ResetTimestampState();

  task_runner_->PostTask(FROM_HERE, std::move(closure));
}

bool ATAudioDecoder::DetectFormat(scoped_refptr<DecoderBuffer> buffer) {
  DCHECK(format_detection_);
  bool ok = RunFormatDetection(format_detection_.get(), std::move(buffer));
  if (ok && !format_detection_->format_ready)
    return true;
  if (ok) {
    ok = InitializeConverter(format_detection_->format, config_.extra_data());
  }
  if (ok) {
    for (scoped_refptr<DecoderBuffer>& queued_buffer :
         format_detection_->queue) {
      ok = ConvertAudio(std::move(queued_buffer));
      if (!ok)
        break;
    }
  }
  format_detection_.reset();
  return ok;
}

bool ATAudioDecoder::InitializeConverter(
    const AudioStreamBasicDescription& input_format,
    const std::vector<uint8_t>& full_extra_data) {
  VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__;
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!converter_);

  GetOutputFormat(input_format, &output_format_);

  OSStatus status =
      AudioConverterNew(&input_format, &output_format_, &converter_);
  if (status != noErr) {
    LOG(ERROR) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
               << ": Failed to create AudioConverter"
               << " Error Status : " << status;
    return false;
  }

  AudioChannelLayout input_channel_layout{};
  if (!GetInputChannelLayout(config_, full_extra_data, input_channel_layout))
    return false;

  status = AudioConverterSetProperty(
      converter_, kAudioConverterInputChannelLayout,
      sizeof(input_channel_layout), &input_channel_layout);

  if (status == kAudio_ParamError &&
      input_channel_layout.mChannelLayoutTag == kAudioChannelLayoutTag_Mono) {
    // Fix for VB-41624 and VB-40534 - Doesn't like Mono as input layout for AAC
    input_channel_layout.mChannelLayoutTag = kAudioChannelLayoutTag_Stereo;
    LOG(WARNING) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
                 << " Changed input layout from Mono to Stereo";
    status = AudioConverterSetProperty(
        converter_, kAudioConverterInputChannelLayout,
        sizeof(input_channel_layout), &input_channel_layout);
  }
  if (status != noErr) {
    LOG(ERROR) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
               << ": Failed to set input channel layout"
               << " Error Status : " << status;
    return false;
  }

  AudioChannelLayout output_channel_layout{};
  output_channel_layout.mChannelLayoutTag =
      ChromeChannelLayoutToCoreAudioTag(config_.channel_layout());

  VLOG(5) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
          << " Input Channel Layout : "
          << Loggable(input_channel_layout.mChannelLayoutTag);
  VLOG(5) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
          << " Output Channel Layout : "
          << Loggable(output_channel_layout.mChannelLayoutTag);

  // Fix for VB-40530 : If output layout is Mono and input layout is not, the
  // below call to AudioConverterSetProperty will fail, so in that case use the
  // input channel layout
  // See also these tests :
  // LegacyByDts/MSEPipelineIntegrationTest.ADTS/0
  // LegacyByDts/MSEPipelineIntegrationTest.ADTS_TimestampOffset/0

  if (output_channel_layout.mChannelLayoutTag == kAudioChannelLayoutTag_Mono &&
      input_channel_layout.mChannelLayoutTag != kAudioChannelLayoutTag_Mono) {
    output_channel_layout.mChannelLayoutTag =
        input_channel_layout.mChannelLayoutTag;
    LOG(WARNING) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
                 << " Changed output layout from Mono to "
                 << Loggable(input_channel_layout.mChannelLayoutTag);
  } else {
    VLOG(5) << " PROPMEDIA(RENDERER) : " << __FUNCTION__ << " Kept layout "
            << Loggable(output_channel_layout.mChannelLayoutTag);
  }

  status = AudioConverterSetProperty(
      converter_, kAudioConverterOutputChannelLayout,
      sizeof(output_channel_layout), &output_channel_layout);
  if (status != noErr) {
    LOG(ERROR) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
               << ": Failed to set output channel layout :"
               << " Error Status : " << status;
    return false;
  }

  return true;
}

void ATAudioDecoder::CloseConverter() {
  if (!converter_)
    return;
  OSStatus status = AudioConverterDispose(converter_);
  if (status != noErr) {
    OSSTATUS_VLOG(1, status) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
                             << ": Failed to dispose of AudioConverter"
                             << " Error Status : " << status;
  }
  converter_ = nullptr;
}

bool ATAudioDecoder::ConvertAudio(scoped_refptr<DecoderBuffer> input) {
  // NOTE(espen@vivaldi.com): Added as crash fix for OSX 10.12
  if (input.get() == nullptr || input->end_of_stream() ||
      input->data_size() == 0) {
    VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__ << " no_data";

    return true;
  }

  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(converter_);

  size_t header_size = 0;
  size_t max_output_frame_count = 0;
  GetHeaderSizeAndMaxFrameCount(config_, header_size, max_output_frame_count);
  UInt32 output_frame_count = max_output_frame_count;

  // Pre-allocate a buffer for the maximum expected frame count.  We will let
  // the AudioConverter fill it with decoded audio, through |output_buffers|
  // defined below.

  ChannelLayout layout = config_.channel_layout();
  int channels = output_format_.mChannelsPerFrame;
  if (channels != config_.channels())
    layout = GuessChannelLayout(channels);

  scoped_refptr<AudioBuffer> output =
      AudioBuffer::CreateBuffer(kOutputSampleFormat, layout, channels,
                                output_format_.mSampleRate, output_frame_count);

  InputData input_data =
      input->end_of_stream()
          // No more input data, but we must flush AudioConverter.
          ? InputData()
          // Will provide data from |input| to AudioConverter in ProvideData().
          : InputData(*input, output->channel_count(), header_size);

  AudioBufferList output_buffers;
  output_buffers.mNumberBuffers = 1;
  output_buffers.mBuffers[0].mNumberChannels = output->channel_count();
  output_buffers.mBuffers[0].mDataByteSize =
      output->frame_count() * output->channel_count() *
      SampleFormatToBytesPerChannel(kOutputSampleFormat);
  // Will put decoded data in the |output| AudioBuffer directly.
  output_buffers.mBuffers[0].mData = output->channel_data()[0];

  AudioStreamPacketDescription
      output_packet_descriptions[max_output_frame_count];

  OSStatus status = AudioConverterFillComplexBuffer(
      converter_, &ProvideData, &input_data, &output_frame_count,
      &output_buffers, output_packet_descriptions);

  if (status != noErr && status != kDataConsumed) {
    OSSTATUS_VLOG(1, status) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
                             << ": Failed to convert audio"
                             << " Error Status : " << status;
    return false;
  }

  if (output_frame_count > max_output_frame_count) {
    VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
            << " Unexpected output sample count: " << output_frame_count;
    return false;
  }

  if (!input->end_of_stream()) {
    queued_input_timing_.push_back(input->time_info());
  }

  if (output_frame_count > 0 && !queued_input_timing_.empty()) {
    output->TrimEnd(max_output_frame_count - output_frame_count);

    DecoderBuffer::TimeInfo dequeued_timing = queued_input_timing_.front();
    queued_input_timing_.pop_front();

    const bool first_output_buffer = !discard_helper_->initialized();
    if (first_output_buffer)
      output = AddFrontPadding(output, config_.codec_delay());

    VLOG(5) << " PROPMEDIA(RENDERER) : " << __FUNCTION__ << " Decoded "
            << output_frame_count << " frames @" << dequeued_timing.timestamp;

    // ProcessBuffers() computes and sets the timestamp on |output|.
    if (discard_helper_->ProcessBuffers(dequeued_timing, output.get())) {
      task_runner_->PostTask(FROM_HERE, base::BindOnce(output_cb_, output));
    }
  }

  return true;
}

void ATAudioDecoder::ResetTimestampState() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
          << " samples_per_second : " << config_.samples_per_second();

  discard_helper_.reset(new AudioDiscardHelper(config_.samples_per_second(),
                                               config_.codec_delay(), false));
  discard_helper_->Reset(config_.codec_delay());

  queued_input_timing_.clear();
}

}  // namespace media
