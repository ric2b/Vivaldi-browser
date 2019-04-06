// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2015 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA.

#include "platform_media/renderer/decoders/win/wmf_decoder_impl.h"

#include <mferror.h>
#include <wmcodecdsp.h>
#include <algorithm>
#include <string>
#include <wrl/client.h>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/numerics/safe_conversions.h"
#include "base/win/windows_version.h"
#include "media/base/audio_buffer.h"
#include "media/base/channel_layout.h"
#include "media/base/data_buffer.h"
#include "media/base/decoder_buffer.h"
#include "platform_media/common/platform_logging_util.h"
#include "platform_media/common/platform_mime_util.h"
#include "media/base/win/mf_initializer.h"
#include "platform_media/common/win/mf_util.h"

namespace media {

namespace {

template <DemuxerStream::Type StreamType>
void ReportInitResult(bool success);

template <>
void ReportInitResult<DemuxerStream::AUDIO>(bool success) {
}

template <>
void ReportInitResult<DemuxerStream::VIDEO>(bool success) {
}

// This function is used as |no_longer_needed_cb| of
// VideoFrame::WrapExternalYuvData to make sure we keep reference to DataBuffer
// object as long as we need it.
void BufferHolder(const scoped_refptr<DataBuffer>& buffer) {
  /* Intentionally empty */
}

SampleFormat ConvertToSampleFormat(uint32_t sample_size) {
  // We set output stream to use MFAudioFormat_PCM. MSDN does not state openly
  // that this is an integer format but there is an example which shows that
  // floating point PCM audio is set using MFAudioFormat_Float subtype for AAC
  // decoder, so we have to assume that for an integer format we should use
  // MFAudioFormat_PCM.
  switch (sample_size) {
    case 1:
      return SampleFormat::kSampleFormatU8;
    case 2:
      return SampleFormat::kSampleFormatS16;
    case 4:
      return SampleFormat::kSampleFormatS32;
    default:
      return kUnknownSampleFormat;
  }
}

int CalculateBufferAlignment(DWORD alignment) {
  return (alignment > 1) ? alignment - 1 : 0;
}

GUID AudioCodecToAudioSubtypeGUID(AudioCodec codec) {
  switch (codec) {
    case AudioCodec::kCodecAAC:
      return MFAudioFormat_AAC;
    default:
      NOTREACHED();
  }
  return GUID();
}

}  // namespace

template <DemuxerStream::Type StreamType>
WMFDecoderImpl<StreamType>::WMFDecoderImpl(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner)
    : task_runner_(task_runner),
      output_sample_size_(0),
      get_stride_function_(nullptr) {}

template <DemuxerStream::Type StreamType>
void WMFDecoderImpl<StreamType>::Initialize(const DecoderConfig& config,
                                            const InitCB& init_cb,
                                            const OutputCB& output_cb) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (!IsValidConfig(config)) {
    VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
            << " Media Config not accepted for codec : "
            << GetCodecName(config.codec());
    init_cb.Run(false);
    return;
  } else {
    VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
            << " Supported decoder config for codec : "
            << Loggable(config);
  }

  InitializeMediaFoundation();

  config_ = config;

  decoder_ = CreateWMFDecoder(config_);
  if (!decoder_ || !ConfigureDecoder()) {
    VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
            << " Creating/Configuring failed for codec : "
            << GetCodecName(config.codec());
    ReportInitResult<StreamType>(false);
    init_cb.Run(false);
    return;
  }

  debug_buffer_logger_.Initialize(GetCodecName(config_.codec()));

  output_cb_ = output_cb;
  ResetTimestampState();

  ReportInitResult<StreamType>(true);
  init_cb.Run(true);
}

template <DemuxerStream::Type StreamType>
void WMFDecoderImpl<StreamType>::Decode(
    scoped_refptr<DecoderBuffer> buffer,
    const DecodeCB& decode_cb) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  debug_buffer_logger_.Log(buffer);

  if (buffer->end_of_stream()) {
    VLOG(5) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
            << " (EOS)";
    const bool drained_ok = Drain();
    LOG_IF(WARNING, !drained_ok)
        << " PROPMEDIA(RENDERER) : " << __FUNCTION__
        << " Drain did not succeed - returning DECODE_ERROR";
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(decode_cb,
                   drained_ok ? DecodeStatus::OK : DecodeStatus::DECODE_ERROR));
    return;
  }
  VLOG(5) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
          << " (" << buffer->timestamp() << ")";

  const HRESULT hr = ProcessInput(buffer);
  DCHECK_NE(MF_E_NOTACCEPTING, hr)
      << "The transform is neither producing output "
         "nor accepting input? This must not happen, see ProcessOutputLoop()";
  typename media::DecodeStatus status = SUCCEEDED(hr) && ProcessOutputLoop()
                                            ? DecodeStatus::OK
                                            : DecodeStatus::DECODE_ERROR;

  LOG_IF(WARNING, (status == DecodeStatus::DECODE_ERROR))
      << " PROPMEDIA(RENDERER) : " << __FUNCTION__
      << " processing buffer failed, returning DECODE_ERROR";

  task_runner_->PostTask(FROM_HERE, base::Bind(decode_cb, status));
}

template <DemuxerStream::Type StreamType>
void WMFDecoderImpl<StreamType>::Reset(const base::Closure& closure) {
  VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  // Transform needs to be reset, skip this and seeking may fail.
  decoder_->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, 0);

  ResetTimestampState();

  task_runner_->PostTask(FROM_HERE, closure);
}

template <>
bool WMFDecoderImpl<DemuxerStream::AUDIO>::IsValidConfig(
    const DecoderConfig& config) {
  if (config.codec() != AudioCodec::kCodecAAC) {
    VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
            << " Unsupported Audio codec : " << GetCodecName(config.codec());
    return false;
  }

  if (config.is_encrypted()) {
    VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
            << " Unsupported Encrypted Audio codec : "
            << GetCodecName(config.codec());
    return false;
  }

  bool isAvailable = IsPlatformAudioDecoderAvailable(config.codec());

  LOG_IF(WARNING, !isAvailable)
      << " PROPMEDIA(RENDERER) : " << __FUNCTION__
      << " Audio Platform Decoder (" << GetCodecName(config.codec())
      << ") : Unavailable";

  return isAvailable;
}

template <>
bool WMFDecoderImpl<DemuxerStream::VIDEO>::IsValidConfig(
    const DecoderConfig& config) {
  if (!IsPlatformVideoDecoderAvailable()) {
    VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
            << " Video Platform Decoder : Unavailable";
    return false;
  }

  LOG_IF(WARNING, !(config.codec() == VideoCodec::kCodecH264))
      << " PROPMEDIA(RENDERER) : " << __FUNCTION__
      << " Unsupported Video codec : " << GetCodecName(config.codec());

  if (config.codec() == VideoCodec::kCodecH264) {
    LOG_IF(WARNING, !(config.profile() >= VideoCodecProfile::H264PROFILE_MIN))
        << " PROPMEDIA(RENDERER) : " << __FUNCTION__
        << " Unsupported Video profile (too low) : " << config.profile();

    LOG_IF(WARNING, !(config.profile() <= VideoCodecProfile::H264PROFILE_MAX))
        << " PROPMEDIA(RENDERER) : " << __FUNCTION__
        << " Unsupported Video profile (too high) : " << config.profile();
  }

  if (config.is_encrypted()) {
    VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
            << " Unsupported Encrypted VIDEO codec : "
            << GetCodecName(config.codec());
    return false;
  }

  return config.codec() == VideoCodec::kCodecH264 && config.profile() >= VideoCodecProfile::H264PROFILE_MIN &&
         config.profile() <= VideoCodecProfile::H264PROFILE_MAX;
}

// static
template <>
std::string WMFDecoderImpl<DemuxerStream::AUDIO>::GetModuleName(
    const DecoderConfig& config) {
  return GetMFAudioDecoderLibraryName(config.codec());
}

// static
template <>
std::string WMFDecoderImpl<DemuxerStream::VIDEO>::GetModuleName(
    const DecoderConfig& /* config */) {
  return GetMFVideoDecoderLibraryName();
}

// static
template <>
GUID WMFDecoderImpl<DemuxerStream::AUDIO>::GetMediaObjectGUID(
    const DecoderConfig& config) {
  switch (config.codec()) {
    case AudioCodec::kCodecAAC:
      return __uuidof(CMSAACDecMFT);
    default:
      NOTREACHED();
  }
  return GUID();
}

// static
template <>
GUID WMFDecoderImpl<DemuxerStream::VIDEO>::GetMediaObjectGUID(
    const DecoderConfig& /* config */) {
  return __uuidof(CMSH264DecoderMFT);
}

// static
template <DemuxerStream::Type StreamType>
Microsoft::WRL::ComPtr<IMFTransform>
WMFDecoderImpl<StreamType>::CreateWMFDecoder(const DecoderConfig& config) {
  Microsoft::WRL::ComPtr<IMFTransform> decoder;

  // CoCreateInstance() is not avaliable in the sandbox, must "reimplement it".
  decltype(DllGetClassObject)* const get_class_object =
      reinterpret_cast<decltype(DllGetClassObject)*>(GetFunctionFromLibrary(
          "DllGetClassObject", GetModuleName(config).c_str()));
  if (!get_class_object) {
    LOG(WARNING) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
                 << " Error while retrieving class object getter function.";
    return decoder;
  }

  Microsoft::WRL::ComPtr<IClassFactory> factory;
  HRESULT hr =
      get_class_object(GetMediaObjectGUID(config), __uuidof(IClassFactory),
                       reinterpret_cast<void**>(factory.GetAddressOf()));
  if (FAILED(hr)) {
    LOG(WARNING) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
                 << " Error while getting class object.";
    return decoder;
  }

  hr = factory->CreateInstance(nullptr, __uuidof(IMFTransform),
                               reinterpret_cast<void**>(decoder.GetAddressOf()));
  if (FAILED(hr)) {
    LOG(WARNING) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
                 << " Error while creating decoder instance.";
    return Microsoft::WRL::ComPtr<IMFTransform>();
  }

  return decoder;
}

template <DemuxerStream::Type StreamType>
bool WMFDecoderImpl<StreamType>::ConfigureDecoder() {
  if (!SetInputMediaType())
    return false;

  if (!SetOutputMediaType())
    return false;

  if (!InitializeDecoderFunctions())
    return false;

  // It requires both input and output to be set.
  HRESULT hr = decoder_->GetInputStreamInfo(0, &input_stream_info_);
  if (FAILED(hr)) {
    LOG(WARNING) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
                 << " Error while getting input stream info.";
    return false;
  }

  return true;
}

template <>
bool WMFDecoderImpl<DemuxerStream::AUDIO>::SetInputMediaType() {
  Microsoft::WRL::ComPtr<IMFMediaType> media_type;
  HRESULT hr = MFCreateMediaType(media_type.GetAddressOf());
  if (FAILED(hr)) {
    LOG(WARNING) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
                 << " Error while creating media type.";
    return false;
  }

  hr = media_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
  if (FAILED(hr)) {
    LOG(WARNING) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
                 << " Error while setting media major type.";
    return false;
  }

  hr = media_type->SetGUID(MF_MT_SUBTYPE,
                           AudioCodecToAudioSubtypeGUID(config_.codec()));
  if (FAILED(hr)) {
    LOG(WARNING) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
                 << " Error while setting media subtype.";
    return false;
  }

  hr = media_type->SetUINT32(
      MF_MT_AUDIO_NUM_CHANNELS,
      ChannelLayoutToChannelCount(config_.channel_layout()));
  if (FAILED(hr)) {
    LOG(WARNING) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
                 << " Error while setting channel number.";
    return false;
  }

  VLOG(5) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
          << " samples_per_second : " << config_.samples_per_second();

  hr = media_type->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND,
                             config_.samples_per_second());
  if (FAILED(hr)) {
    LOG(WARNING) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
                 << " Error while setting samples per second.";
    return false;
  }

  if (config_.codec() == AudioCodec::kCodecAAC) {
    hr = media_type->SetUINT32(MF_MT_AAC_PAYLOAD_TYPE, 0x1);
    if (FAILED(hr)) {
      LOG(WARNING) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
                   << " Error while setting AAC payload type.";
      return false;
    }

    // AAC decoder requires setting up HEAACWAVEINFO as a MF_MT_USER_DATA,
    // without this decoder fails to work (e.g. ProcessOutput returns
    // repeatedly with mysterious MF_E_TRANSFORM_STREAM_CHANGE status).
    // mt_user_data size is 12 = size of relevant fields of HEAACWAVEINFO
    // structure, see
    // http://msdn.microsoft.com/en-us/library/windows/desktop/dd742784%28v=vs.85%29.aspx
    uint8_t mt_user_data[12] = {1};  // Set input type to adts.
    hr = media_type->SetBlob(MF_MT_USER_DATA, mt_user_data,
                             sizeof(mt_user_data));
    if (FAILED(hr)) {
      LOG(WARNING) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
                   << " Error while setting AAC AudioSpecificConfig().";
      return false;
    }
  }

  hr = decoder_->SetInputType(0, media_type.Get(), 0);  // No flags.
  if (FAILED(hr)) {
    std::string error_code;
    switch (hr) {
      case S_OK:
        error_code = "S_OK";
        break;
      case MF_E_INVALIDMEDIATYPE:
        error_code = "MF_E_INVALIDMEDIATYPE";
        break;
      case MF_E_INVALIDSTREAMNUMBER:
        error_code = "MF_E_INVALIDSTREAMNUMBER";
        break;
      case MF_E_INVALIDTYPE:
        error_code = "MF_E_INVALIDTYPE";
        break;
      case MF_E_TRANSFORM_CANNOT_CHANGE_MEDIATYPE_WHILE_PROCESSING:
        error_code = "MF_E_TRANSFORM_CANNOT_CHANGE_MEDIATYPE_WHILE_PROCESSING";
        break;
      case MF_E_TRANSFORM_TYPE_NOT_SET:
        error_code = "MF_E_TRANSFORM_TYPE_NOT_SET";
        break;
      case MF_E_UNSUPPORTED_D3D_TYPE:
        error_code = "MF_E_UNSUPPORTED_D3D_TYPE";
        break;
    }

    VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
            << " Error while setting input type : " << error_code;
    return false;
  }

  return true;
}

template <>
bool WMFDecoderImpl<DemuxerStream::VIDEO>::SetInputMediaType() {
  Microsoft::WRL::ComPtr<IMFMediaType> media_type;
  HRESULT hr = MFCreateMediaType(media_type.GetAddressOf());
  if (FAILED(hr)) {
    LOG(WARNING) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
                 << " Error while creating media type.";
    return false;
  }

  hr = media_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
  if (FAILED(hr)) {
    LOG(WARNING) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
                 << " Error while setting media major type.";
    return false;
  }

  hr = media_type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
  if (FAILED(hr)) {
    LOG(WARNING) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
                 << " Error while setting media subtype.";
    return false;
  }

  hr = media_type->SetUINT32(MF_MT_INTERLACE_MODE,
                             MFVideoInterlace_MixedInterlaceOrProgressive);
  if (FAILED(hr)) {
    LOG(WARNING) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
                 << " Error while setting interlace mode.";
    return false;
  }

  hr = MFSetAttributeSize(media_type.Get(), MF_MT_FRAME_SIZE,
                          config_.coded_size().width(),
                          config_.coded_size().height());
  if (FAILED(hr)) {
    LOG(WARNING) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
                 << " Error while setting frame size.";
    return false;
  }

  hr = decoder_->SetInputType(0, media_type.Get(), 0);  // No flags.
  if (FAILED(hr)) {
    LOG(WARNING) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
                 << " Error while setting input type.";
    return false;
  }

  return true;
}

template <DemuxerStream::Type StreamType>
bool WMFDecoderImpl<StreamType>::SetOutputMediaType() {
  VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__;

  Microsoft::WRL::ComPtr<IMFMediaType> out_media_type;

  HRESULT hr = S_OK;
  for (uint32_t i = 0; SUCCEEDED(
           decoder_->GetOutputAvailableType(0, i, out_media_type.GetAddressOf()));
       ++i) {
    GUID out_subtype = {0};
    hr = out_media_type->GetGUID(MF_MT_SUBTYPE, &out_subtype);
    if (FAILED(hr)) {
      LOG(WARNING) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
                   << " Error while getting available output types.";
      return false;
    }

    hr = SetOutputMediaTypeInternal(out_subtype, out_media_type.Get());
    if (hr == S_OK) {
      break;
    } else if (hr != S_FALSE) {
      LOG(WARNING) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
                   << " SetOutputMediaTypeInternal returned an error";
      return false;
    }

    out_media_type.Reset();
  }

  MFT_OUTPUT_STREAM_INFO output_stream_info = {0};
  hr = decoder_->GetOutputStreamInfo(0, &output_stream_info);
  if (FAILED(hr)) {
    LOG(WARNING) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
                 << " Error while getting stream info.";
    return false;
  }

  output_sample_ = nullptr;
  const bool decoder_creates_samples =
      (output_stream_info.dwFlags & (MFT_OUTPUT_STREAM_PROVIDES_SAMPLES |
                                     MFT_OUTPUT_STREAM_CAN_PROVIDE_SAMPLES)) !=
      0;
  if (!decoder_creates_samples) {
    output_sample_ =
        CreateSample(CalculateOutputBufferSize(output_stream_info),
                     CalculateBufferAlignment(output_stream_info.cbAlignment));
    if (!output_sample_) {
      VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
              << " Couldn't create sample";
      return false;
    }
  }

  return true;
}

template <>
HRESULT WMFDecoderImpl<DemuxerStream::AUDIO>::SetOutputMediaTypeInternal(
    GUID subtype,
    IMFMediaType* media_type) {
  if (subtype == MFAudioFormat_PCM) {
    HRESULT hr = decoder_->SetOutputType(0, media_type, 0);  // No flags.
    if (FAILED(hr)) {
      LOG(WARNING) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
                   << " Error while setting output type.";
      return hr;
    }

    hr = media_type->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND,
                               &output_samples_per_second_);
    if (FAILED(hr)) {
      LOG(WARNING) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
                   << " Error while getting sample rate.";
      return hr;
    }

    uint32_t output_channel_count;
    hr = media_type->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &output_channel_count);
    if (FAILED(hr)) {
      LOG(WARNING) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
                   << " Error while getting sample rate.";
      return hr;
    }

    if (base::checked_cast<int>(output_channel_count) == config_.channels())
      output_channel_layout_ = config_.channel_layout();
    else
      output_channel_layout_ = GuessChannelLayout(output_channel_count);

    hr = media_type->GetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE,
                               &output_sample_size_);
    if (FAILED(hr)) {
      LOG(WARNING) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
                   << " Error while getting sample size.";
      return hr;
    }
    // We will need size in Bytes.
    output_sample_size_ /= 8;
    return S_OK;
  }
  return S_FALSE;
}

template <>
HRESULT WMFDecoderImpl<DemuxerStream::VIDEO>::SetOutputMediaTypeInternal(
    GUID subtype,
    IMFMediaType* media_type) {
  if (subtype == MFVideoFormat_YV12) {
    HRESULT hr = decoder_->SetOutputType(0, media_type, 0);  // No flags.
    if (FAILED(hr)) {
      LOG(WARNING) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
                   << " Error while setting output type.";
      return hr;
    }
    return S_OK;
  }
  return S_FALSE;
}

template <>
size_t WMFDecoderImpl<DemuxerStream::AUDIO>::CalculateOutputBufferSize(
    const MFT_OUTPUT_STREAM_INFO& stream_info) const {
  size_t buffer_size = stream_info.cbSize;
  return buffer_size;
}

template <>
size_t WMFDecoderImpl<DemuxerStream::VIDEO>::CalculateOutputBufferSize(
    const MFT_OUTPUT_STREAM_INFO& stream_info) const {
  return stream_info.cbSize;
}

template <>
bool WMFDecoderImpl<DemuxerStream::AUDIO>::InitializeDecoderFunctions() {
  return true;
}

template <>
bool WMFDecoderImpl<DemuxerStream::VIDEO>::InitializeDecoderFunctions() {
  get_stride_function_ = reinterpret_cast<decltype(get_stride_function_)>(
      GetFunctionFromLibrary("MFGetStrideForBitmapInfoHeader", "evr.dll"));
  return get_stride_function_ != nullptr;
}

template <DemuxerStream::Type StreamType>
HRESULT WMFDecoderImpl<StreamType>::ProcessInput(
    const scoped_refptr<DecoderBuffer>& input) {
  VLOG(5) << " PROPMEDIA(RENDERER) : " << __FUNCTION__;
  DCHECK(input.get());

  const Microsoft::WRL::ComPtr<IMFSample> sample =
      PrepareInputSample(input.get());
  if (!sample) {
    VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
            << " Failed to create input sample.";
    return MF_E_UNEXPECTED;
  }

  const HRESULT hr = decoder_->ProcessInput(0, sample.Get(), 0);

  if (SUCCEEDED(hr))
    RecordInput(input);

  return hr;
}

template <>
void WMFDecoderImpl<DemuxerStream::AUDIO>::RecordInput(
    const scoped_refptr<DecoderBuffer>& input) {
  // We use AudioDiscardHelper to calculate output audio timestamps and discard
  // output buffers per the instructions in DecoderBuffer.  AudioDiscardHelper
  // needs both the output buffers and the corresponsing input buffers to do
  // its work, so we need to queue the input buffers to cover the case when
  // Decode() doesn't produce output immediately.
  queued_input_.push_back(input);
}

template <>
void WMFDecoderImpl<DemuxerStream::VIDEO>::RecordInput(
    const scoped_refptr<DecoderBuffer>& input) {
  // Do nothing.  We obtain timestamps from IMFTransform::GetSampleTime() for
  // video.
}

template <DemuxerStream::Type StreamType>
HRESULT WMFDecoderImpl<StreamType>::ProcessOutput() {
  VLOG(5) << " PROPMEDIA(RENDERER) : " << __FUNCTION__;

  // Make the whole buffer available for use by |decoder_| again after it was
  // filled with data by the previous call to ProcessOutput().
  IMFMediaBuffer* buffer = nullptr;
  HRESULT hr = output_sample_->ConvertToContiguousBuffer(&buffer);
  if (FAILED(hr)) {
    LOG(WARNING) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
                 << " Error while converting buffer.";
    return hr;
  }

  hr = buffer->SetCurrentLength(0);
  if (FAILED(hr)) {
    LOG(WARNING) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
                 << " Error while setting buffer length.";
    return hr;
  }

  MFT_OUTPUT_DATA_BUFFER output_data_buffer = {0};
  output_data_buffer.pSample = output_sample_.Get();

  DWORD process_output_status = 0;
  hr = decoder_->ProcessOutput(0, 1, &output_data_buffer,
                               &process_output_status);
  IMFCollection* events = output_data_buffer.pEvents;
  if (events != nullptr) {
    // Even though we're not interested in events we have to clean them up.
    events->Release();
  }

  switch (hr) {
    case S_OK: {
      scoped_refptr<OutputType> output_buffer =
          CreateOutputBuffer(output_data_buffer);
      if (!output_buffer)
        return MF_E_UNEXPECTED;

      if (!ProcessBuffer(output_buffer))
        break;

      if (!output_cb_.is_null()) {
        task_runner_->PostTask(FROM_HERE,
                               base::Bind(output_cb_, output_buffer));
      } else {
        return E_ABORT;
      }

      break;
    }
    case MF_E_TRANSFORM_NEED_MORE_INPUT:
      VLOG(5) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
              << " NEED_MORE_INPUT";
      // Need to wait for more input data to produce output.
      break;
    case MF_E_TRANSFORM_STREAM_CHANGE:
      VLOG(5) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
              << " STREAM_CHANGE";
      // For some reason we need to set up output media type again.
      if (!SetOutputMediaType())
        return MF_E_UNEXPECTED;
      // This kind of change will probably prevent us from getting more output.
      break;
  }

  return hr;
}

template <>
bool WMFDecoderImpl<DemuxerStream::AUDIO>::ProcessBuffer(
    const scoped_refptr<AudioBuffer>& output) {
  if (queued_input_.empty())
    return false;

  const scoped_refptr<DecoderBuffer> dequeued_input = queued_input_.front();
  queued_input_.pop_front();

  return discard_helper_->ProcessBuffers(*dequeued_input, output);
}

template <>
bool WMFDecoderImpl<DemuxerStream::VIDEO>::ProcessBuffer(
    const scoped_refptr<VideoFrame>& /* output */) {
  // Nothing to do.
  return true;
}

template <DemuxerStream::Type StreamType>
bool WMFDecoderImpl<StreamType>::ProcessOutputLoop() {
  for (;;) {
    const HRESULT hr = ProcessOutput();
    if (FAILED(hr)) {
      // If ProcessOutput fails with MF_E_TRANSFORM_NEED_MORE_INPUT or
      // MF_E_TRANSFORM_STREAM_CHANGE, it means it failed to get any output, but
      // still this is not a decoding error - the decoder just needs more input
      // data or reconfiguration on stream format change, so those errors do not
      // mean that ProcessOutputLoop failed.
      if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT)
        return true;

      if (hr == MF_E_TRANSFORM_STREAM_CHANGE)
        continue;

      LOG(WARNING) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
                   << " ProcessOutput failed with an error.";
      return false;
    }
  }
}

template <DemuxerStream::Type StreamType>
bool WMFDecoderImpl<StreamType>::Drain() {
  decoder_->ProcessMessage(MFT_MESSAGE_COMMAND_DRAIN, 0);
  return ProcessOutputLoop();
}

template <DemuxerStream::Type StreamType>
Microsoft::WRL::ComPtr<IMFSample>
WMFDecoderImpl<StreamType>::PrepareInputSample(
    const scoped_refptr<DecoderBuffer>& input) const {
  Microsoft::WRL::ComPtr<IMFSample> sample =
      CreateSample(input->data_size(),
                   CalculateBufferAlignment(input_stream_info_.cbAlignment));
  if (!sample) {
    LOG(WARNING) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
                 << " Error while creating sample.";
    return Microsoft::WRL::ComPtr<IMFSample>();
  }

  IMFMediaBuffer* buffer = nullptr;
  HRESULT hr = sample->GetBufferByIndex(0, &buffer);
  if (FAILED(hr)) {
    LOG(WARNING) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
                 << " Error while getting buffer.";
    return Microsoft::WRL::ComPtr<IMFSample>();
  }

  uint8_t* buff_ptr = nullptr;
  hr = buffer->Lock(&buff_ptr, nullptr, nullptr);
  if (FAILED(hr)) {
    LOG(WARNING) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
                 << " Error while locking buffer.";
    return Microsoft::WRL::ComPtr<IMFSample>();
  }

  memcpy(buff_ptr, input->data(), input->data_size());

  hr = buffer->Unlock();
  if (FAILED(hr)) {
    LOG(WARNING) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
                 << " Error while unlocking buffer.";
    return Microsoft::WRL::ComPtr<IMFSample>();
  }

  hr = buffer->SetCurrentLength(input->data_size());
  if (FAILED(hr)) {
    LOG(WARNING) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
                 << " Error while setting buffer length.";
    return Microsoft::WRL::ComPtr<IMFSample>();
  }
  // IMFSample's timestamp is expressed in hundreds of nanoseconds.
  hr = sample->SetSampleTime(input->timestamp().InMicroseconds() * 10);
  if (FAILED(hr)) {
    LOG(WARNING) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
                 << " Error while setting sample timestamp.";
    return Microsoft::WRL::ComPtr<IMFSample>();
  }

  return sample;
}

template <DemuxerStream::Type StreamType>
scoped_refptr<typename WMFDecoderImpl<StreamType>::OutputType>
WMFDecoderImpl<StreamType>::CreateOutputBuffer(
    const MFT_OUTPUT_DATA_BUFFER& output_data_buffer) {
  Microsoft::WRL::ComPtr<IMFMediaBuffer> media_buffer;
  HRESULT hr = output_data_buffer.pSample->ConvertToContiguousBuffer(
      media_buffer.GetAddressOf());
  if (FAILED(hr)) {
    LOG(WARNING) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
                 << " Error while converting buffer.";
    return nullptr;
  }

  uint8_t* data = nullptr;
  DWORD data_size = 0;
  hr = media_buffer->Lock(&data, nullptr, &data_size);
  if (FAILED(hr)) {
    LOG(WARNING) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
                 << " Error while locking buffer.";
    return nullptr;
  }

  LONGLONG sample_time = 0;
  hr = output_data_buffer.pSample->GetSampleTime(&sample_time);
  if (FAILED(hr)) {
    LOG(WARNING) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
                 << " Error while getting sample time.";
    return nullptr;
  }
  // The sample time in IMFSample is expressed in hundreds of nanoseconds.
  const base::TimeDelta timestamp =
      base::TimeDelta::FromMicroseconds(sample_time / 10);

  scoped_refptr<typename WMFDecoderImpl<StreamType>::OutputType> output =
      CreateOutputBufferInternal(data, data_size, timestamp);

  media_buffer->Unlock();
  return output;
}

template <>
scoped_refptr<AudioBuffer>
WMFDecoderImpl<DemuxerStream::AUDIO>::CreateOutputBufferInternal(
    const uint8_t* data,
    DWORD data_size,
    base::TimeDelta /* timestamp */) {
  DCHECK_GT(output_sample_size_, 0u) << "Division by zero";

  const int frame_count = data_size / output_sample_size_ /
                          ChannelLayoutToChannelCount(output_channel_layout_);

  // The timestamp will be calculated by |discard_helper_| later on.

  VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
          << " samples_per_second : " << config_.samples_per_second();

  return AudioBuffer::CopyFrom(
      ConvertToSampleFormat(output_sample_size_), output_channel_layout_,
      ChannelLayoutToChannelCount(output_channel_layout_),
      output_samples_per_second_, frame_count, &data, kNoTimestamp);
}

template <>
scoped_refptr<VideoFrame>
WMFDecoderImpl<DemuxerStream::VIDEO>::CreateOutputBufferInternal(
    const uint8_t* data,
    DWORD data_size,
    base::TimeDelta timestamp) {
  const scoped_refptr<DataBuffer> data_buffer =
      DataBuffer::CopyFrom(data, data_size);

  LONG stride = 0;
  HRESULT hr = get_stride_function_(MFVideoFormat_YV12.Data1,
                                    config_.coded_size().width(), &stride);

  if (FAILED(hr)) {
    LOG(WARNING) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
                 << " Failed to obtain stride.";
    return nullptr;
  }

  // Stride has to be divisible by 16.
  LONG adjusted_stride = ((stride + 15) & ~15);

  if (stride != adjusted_stride) {
    // patricia@vivaldi.com : I don't know why we do this and it smells fishy
      LOG(WARNING) << __FUNCTION__ << " Before Stride : " << stride;
      stride = adjusted_stride;
      LOG(WARNING) << __FUNCTION__ << " After Stride : " << stride;
  }

  // Number of rows has to be divisible by 16.
  LONG rows = config_.coded_size().height();
  LONG adjusted_rows = ((rows + 15) & ~15);

  if (rows != adjusted_rows) {
    // patricia@vivaldi.com : I don't know why we do this and it smells fishy
    LOG(WARNING) << __FUNCTION__ << " Before rows : " << rows;
    rows = adjusted_rows;
    LOG(WARNING) << __FUNCTION__ << " After rows : " << rows;
  }

  scoped_refptr<VideoFrame> frame = VideoFrame::WrapExternalYuvData(
      VideoPixelFormat::PIXEL_FORMAT_YV12, config_.coded_size(), config_.visible_rect(),
      config_.natural_size(), stride, stride / 2, stride / 2,
      const_cast<uint8_t*>(data_buffer->data()),
      const_cast<uint8_t*>(data_buffer->data() +
                           (rows * stride + rows * stride / 4)),
      const_cast<uint8_t*>(data_buffer->data() + (rows * stride)), timestamp);
  frame->AddDestructionObserver(base::Bind(&BufferHolder, data_buffer));
  return frame;
}

template <DemuxerStream::Type StreamType>
Microsoft::WRL::ComPtr<IMFSample> WMFDecoderImpl<StreamType>::CreateSample(
    DWORD buffer_size,
    int buffer_alignment) const {
  Microsoft::WRL::ComPtr<IMFSample> sample;
  HRESULT hr = MFCreateSample(sample.GetAddressOf());
  if (FAILED(hr)) {
    LOG(WARNING) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
                 << " Error while creating sample.";
    return Microsoft::WRL::ComPtr<IMFSample>();
  }

  Microsoft::WRL::ComPtr<IMFMediaBuffer> buffer;
  hr = MFCreateAlignedMemoryBuffer(buffer_size, buffer_alignment,
                                   buffer.GetAddressOf());
  if (FAILED(hr)) {
    LOG(WARNING) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
                 << " Error while creating buffer.";
    return Microsoft::WRL::ComPtr<IMFSample>();
  }

  hr = sample->AddBuffer(buffer.Get());
  if (FAILED(hr)) {
    LOG(WARNING) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
                 << " Error while adding buffer.";
    return Microsoft::WRL::ComPtr<IMFSample>();
  }

  return sample;
}

template <>
void WMFDecoderImpl<DemuxerStream::AUDIO>::ResetTimestampState() {

  VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
          << " samples_per_second : " << config_.samples_per_second();

  discard_helper_.reset(new AudioDiscardHelper(config_.samples_per_second(),
                                               config_.codec_delay(), false));
  discard_helper_->Reset(config_.codec_delay());

  queued_input_.clear();
}

template <>
void WMFDecoderImpl<DemuxerStream::VIDEO>::ResetTimestampState() {
  // Nothing to do.
}

template class WMFDecoderImpl<DemuxerStream::AUDIO>;
template class WMFDecoderImpl<DemuxerStream::VIDEO>;

}  // namespace media
