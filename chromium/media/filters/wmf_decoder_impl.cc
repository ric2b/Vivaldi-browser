// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (C) 2015 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA.

#include "media/filters/wmf_decoder_impl.h"

#include <mferror.h>
#include <wmcodecdsp.h>
#include <algorithm>
#include <string>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/features/features.h"
#include "base/message_loop/message_loop.h"
#include "base/numerics/safe_conversions.h"
#include "base/win/scoped_comptr.h"
#include "base/win/windows_version.h"
#include "media/base/audio_buffer.h"
#include "media/base/channel_layout.h"
#include "media/base/data_buffer.h"
#include "media/base/decoder_buffer.h"
#include "media/base/pipeline_stats.h"
#include "media/base/platform_mime_util.h"
#include "media/base/win/mf_initializer.h"
#include "media/base/win/mf_util.h"

namespace media {

namespace {

template <DemuxerStream::Type StreamType>
void ReportInitResult(bool success);

template <>
void ReportInitResult<DemuxerStream::AUDIO>(bool success) {
  pipeline_stats::ReportAudioDecoderInitResult(success);
}

template <>
void ReportInitResult<DemuxerStream::VIDEO>(bool success) {
  pipeline_stats::ReportVideoDecoderInitResult(success);
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
      return kSampleFormatU8;
    case 2:
      return kSampleFormatS16;
    case 4:
      return kSampleFormatS32;
    default:
      return kUnknownSampleFormat;
  }
}

int CalculateBufferAlignment(DWORD alignment) {
  return (alignment > 1) ? alignment - 1 : 0;
}

GUID AudioCodecToAudioSubtypeGUID(AudioCodec codec) {
  switch (codec) {
    case kCodecAAC:
      return MFAudioFormat_AAC;
    case kCodecMP3:
      return MFAudioFormat_MP3;
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
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (!IsValidConfig(config)) {
    DVLOG(1) << "Unsupported decoder config";
    init_cb.Run(false);
    return;
  }

  InitializeMediaFoundation();

  config_ = config;

  decoder_ = CreateWMFDecoder(config_);
  if (!decoder_ || !ConfigureDecoder()) {
    ReportInitResult<StreamType>(false);
    init_cb.Run(false);
    return;
  }

  output_cb_ = output_cb;
  ResetTimestampState();

  ReportInitResult<StreamType>(true);
  init_cb.Run(true);
}

template <DemuxerStream::Type StreamType>
void WMFDecoderImpl<StreamType>::Decode(
    const scoped_refptr<DecoderBuffer>& buffer,
    const DecodeCB& decode_cb) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (buffer->end_of_stream()) {
    DVLOG(5) << __FUNCTION__ << "(EOS)";
    const bool drained_ok = Drain();
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(decode_cb,
                   drained_ok ? media::DecodeStatus::OK : media::DecodeStatus::DECODE_ERROR));
    return;
  }
  DVLOG(5) << __FUNCTION__ << "(" << buffer->timestamp() << ")";

  const HRESULT hr = ProcessInput(buffer);
  DCHECK_NE(MF_E_NOTACCEPTING, hr)
      << "The transform is neither producing output "
         "nor accepting input? This must not happen, see ProcessOutputLoop()";
  typename media::DecodeStatus status = SUCCEEDED(hr) && ProcessOutputLoop()
                                            ? media::DecodeStatus::OK
                                            : media::DecodeStatus::DECODE_ERROR;

  if (status == media::DecodeStatus::OK &&
      buffer->splice_timestamp() != kNoTimestamp()) {
    DVLOG(1) << "Splice detected, must drain the decoder";
    if (!Drain())
      status = media::DecodeStatus::DECODE_ERROR;
  }

  task_runner_->PostTask(FROM_HERE, base::Bind(decode_cb, status));
}

template <DemuxerStream::Type StreamType>
void WMFDecoderImpl<StreamType>::Reset(const base::Closure& closure) {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  // Transform needs to be reset, skip this and seeking may fail.
  decoder_->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, 0);

  ResetTimestampState();

  task_runner_->PostTask(FROM_HERE, closure);
}

template <>
bool WMFDecoderImpl<DemuxerStream::AUDIO>::IsValidConfig(
    const DecoderConfig& config) {
  if (config.codec() == kCodecMP3 &&
      !base::IsFeatureEnabled(base::kFeatureMseAudioMpegAac)) {
    return false;
  }

  if (config.codec() != kCodecMP3 && config.codec() != kCodecAAC)
    return false;

  return IsPlatformAudioDecoderAvailable(config.codec());
}

template <>
bool WMFDecoderImpl<DemuxerStream::VIDEO>::IsValidConfig(
    const DecoderConfig& config) {
  if (!IsPlatformVideoDecoderAvailable())
    return false;

  return config.codec() == kCodecH264 && config.profile() >= H264PROFILE_MIN &&
         config.profile() <= H264PROFILE_MAX;
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
    case kCodecAAC:
      return __uuidof(CMSAACDecMFT);
    case kCodecMP3:
      return __uuidof(CMP3DecMediaObject);
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
base::win::ScopedComPtr<IMFTransform>
WMFDecoderImpl<StreamType>::CreateWMFDecoder(const DecoderConfig& config) {
  base::win::ScopedComPtr<IMFTransform> decoder;

  // CoCreateInstance() is not avaliable in the sandbox, must "reimplement it".
  decltype(DllGetClassObject)* const get_class_object =
      reinterpret_cast<decltype(DllGetClassObject)*>(GetFunctionFromLibrary(
          "DllGetClassObject", GetModuleName(config).c_str()));
  if (!get_class_object) {
    DVLOG(1) << "Error while retrieving class object getter function.";
    return decoder;
  }

  base::win::ScopedComPtr<IClassFactory> factory;
  HRESULT hr =
      get_class_object(GetMediaObjectGUID(config), __uuidof(IClassFactory),
                       reinterpret_cast<void**>(factory.Receive()));
  if (FAILED(hr)) {
    DVLOG(1) << "Error while getting class object.";
    return decoder;
  }

  hr = factory->CreateInstance(nullptr, __uuidof(IMFTransform),
                               reinterpret_cast<void**>(decoder.Receive()));
  if (FAILED(hr)) {
    DVLOG(1) << "Error while creating decoder instance.";
    return base::win::ScopedComPtr<IMFTransform>();
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
    DVLOG(1) << "Error while getting input stream info.";
    return false;
  }

  return true;
}

template <>
bool WMFDecoderImpl<DemuxerStream::AUDIO>::SetInputMediaType() {
  base::win::ScopedComPtr<IMFMediaType> media_type;
  HRESULT hr = MFCreateMediaType(media_type.Receive());
  if (FAILED(hr)) {
    DVLOG(1) << "Error while creating media type.";
    return false;
  }

  hr = media_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
  if (FAILED(hr)) {
    DVLOG(1) << "Error while setting media major type.";
    return false;
  }

  hr = media_type->SetGUID(MF_MT_SUBTYPE,
                           AudioCodecToAudioSubtypeGUID(config_.codec()));
  if (FAILED(hr)) {
    DVLOG(1) << "Error while setting media subtype.";
    return false;
  }

  hr = media_type->SetUINT32(
      MF_MT_AUDIO_NUM_CHANNELS,
      ChannelLayoutToChannelCount(config_.channel_layout()));
  if (FAILED(hr)) {
    DVLOG(1) << "Error while setting channel number.";
    return false;
  }

  hr = media_type->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND,
                             config_.input_samples_per_second());
  if (FAILED(hr)) {
    DVLOG(1) << "Error while setting samples per second.";
    return false;
  }

  if (config_.codec() == kCodecAAC) {
    hr = media_type->SetUINT32(MF_MT_AAC_PAYLOAD_TYPE, 0x1);
    if (FAILED(hr)) {
      DVLOG(1) << "Error while setting AAC payload type.";
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
      DVLOG(1) << "Error while setting AAC AudioSpecificConfig().";
      return false;
    }
  }

  hr = decoder_->SetInputType(0, media_type.get(), 0);  // No flags.
  if (FAILED(hr)) {
    DVLOG(1) << "Error while setting input type.";
    return false;
  }

  return true;
}

template <>
bool WMFDecoderImpl<DemuxerStream::VIDEO>::SetInputMediaType() {
  base::win::ScopedComPtr<IMFMediaType> media_type;
  HRESULT hr = MFCreateMediaType(media_type.Receive());
  if (FAILED(hr)) {
    DVLOG(1) << "Error while creating media type.";
    return false;
  }

  hr = media_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
  if (FAILED(hr)) {
    DVLOG(1) << "Error while setting media major type.";
    return false;
  }

  hr = media_type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
  if (FAILED(hr)) {
    DVLOG(1) << "Error while setting media subtype.";
    return false;
  }

  hr = media_type->SetUINT32(MF_MT_INTERLACE_MODE,
                             MFVideoInterlace_MixedInterlaceOrProgressive);
  if (FAILED(hr)) {
    DVLOG(1) << "Error while setting interlace mode.";
    return false;
  }

  hr = MFSetAttributeSize(media_type.get(), MF_MT_FRAME_SIZE,
                          config_.coded_size().width(),
                          config_.coded_size().height());
  if (FAILED(hr)) {
    DVLOG(1) << "Error while setting frame size.";
    return false;
  }

  hr = decoder_->SetInputType(0, media_type.get(), 0);  // No flags.
  if (FAILED(hr)) {
    DVLOG(1) << "Error while setting input type.";
    return false;
  }

  return true;
}

template <DemuxerStream::Type StreamType>
bool WMFDecoderImpl<StreamType>::SetOutputMediaType() {
  DVLOG(1) << __FUNCTION__;

  base::win::ScopedComPtr<IMFMediaType> out_media_type;

  HRESULT hr = S_OK;
  for (uint32_t i = 0; SUCCEEDED(
           decoder_->GetOutputAvailableType(0, i, out_media_type.Receive()));
       ++i) {
    GUID out_subtype = {0};
    hr = out_media_type->GetGUID(MF_MT_SUBTYPE, &out_subtype);
    if (FAILED(hr)) {
      DVLOG(1) << "Error while getting available output types.";
      return false;
    }

    hr = SetOutputMediaTypeInternal(out_subtype, out_media_type.get());
    if (hr == S_OK) {
      break;
    } else if (hr != S_FALSE) {
      return false;
    }

    out_media_type.Release();
  }

  MFT_OUTPUT_STREAM_INFO output_stream_info = {0};
  hr = decoder_->GetOutputStreamInfo(0, &output_stream_info);
  if (FAILED(hr)) {
    DVLOG(1) << "Error while getting stream info.";
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
      DVLOG(1) << "Couldn't create sample";
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
      DVLOG(1) << "Error while setting output type.";
      return hr;
    }

    hr = media_type->GetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE,
                               &output_sample_size_);
    if (FAILED(hr)) {
      DVLOG(1) << "Error while getting sample size.";
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
      DVLOG(1) << "Error while setting output type.";
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

  // Limit the buffer size for decoded MP3 audio so that |decoder_| doesn't
  // output more than the MP3 frame size at a time.  This makes us behave more
  // like FFmpeg and allows us to handle timestamp calculations and buffer
  // discards using AudioDiscardHelper.
  if (config_.codec() == kCodecMP3) {
    // http://teslabs.com/openplayer/docs/docs/specs/mp3_structure2.pdf
    static const size_t kMaxOutputSampleCount = 1152;
    buffer_size = std::min(
        buffer_size, kMaxOutputSampleCount *
                         ChannelLayoutToChannelCount(config_.channel_layout()) *
                         output_sample_size_);
  }

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
  DVLOG(5) << __FUNCTION__;
  DCHECK(input.get());

  const base::win::ScopedComPtr<IMFSample> sample =
      PrepareInputSample(input.get());
  if (!sample) {
    DVLOG(1) << "Failed to create input sample.";
    return MF_E_UNEXPECTED;
  }

  const HRESULT hr = decoder_->ProcessInput(0, sample.get(), 0);

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
  DVLOG(5) << __FUNCTION__;

  // Make the whole buffer available for use by |decoder_| again after it was
  // filled with data by the previous call to ProcessOutput().
  IMFMediaBuffer* buffer = nullptr;
  HRESULT hr = output_sample_->ConvertToContiguousBuffer(&buffer);
  if (FAILED(hr))
    return hr;
  hr = buffer->SetCurrentLength(0);
  if (FAILED(hr))
    return hr;

  MFT_OUTPUT_DATA_BUFFER output_data_buffer = {0};
  output_data_buffer.pSample = output_sample_.get();

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
      DVLOG(5) << "NEED_MORE_INPUT";
      // Need to wait for more input data to produce output.
      break;
    case MF_E_TRANSFORM_STREAM_CHANGE:
      DVLOG(5) << "STREAM_CHANGE";
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

  return discard_helper_->ProcessBuffers(dequeued_input, output);
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
base::win::ScopedComPtr<IMFSample>
WMFDecoderImpl<StreamType>::PrepareInputSample(
    const scoped_refptr<DecoderBuffer>& input) const {
  base::win::ScopedComPtr<IMFSample> sample =
      CreateSample(input->data_size(),
                   CalculateBufferAlignment(input_stream_info_.cbAlignment));
  if (!sample)
    return base::win::ScopedComPtr<IMFSample>();

  IMFMediaBuffer* buffer = nullptr;
  HRESULT hr = sample->GetBufferByIndex(0, &buffer);
  if (FAILED(hr))
    return base::win::ScopedComPtr<IMFSample>();

  uint8_t* buff_ptr = nullptr;
  hr = buffer->Lock(&buff_ptr, nullptr, nullptr);
  if (FAILED(hr))
    return base::win::ScopedComPtr<IMFSample>();

  memcpy(buff_ptr, input->data(), input->data_size());

  hr = buffer->Unlock();
  if (FAILED(hr))
    return base::win::ScopedComPtr<IMFSample>();

  hr = buffer->SetCurrentLength(input->data_size());
  if (FAILED(hr))
    return base::win::ScopedComPtr<IMFSample>();

  // IMFSample's timestamp is expressed in hundreds of nanoseconds.
  hr = sample->SetSampleTime(input->timestamp().InMicroseconds() * 10);
  if (FAILED(hr))
    return base::win::ScopedComPtr<IMFSample>();

  return sample;
}

template <DemuxerStream::Type StreamType>
scoped_refptr<typename WMFDecoderImpl<StreamType>::OutputType>
WMFDecoderImpl<StreamType>::CreateOutputBuffer(
    const MFT_OUTPUT_DATA_BUFFER& output_data_buffer) {
  base::win::ScopedComPtr<IMFMediaBuffer> media_buffer;
  HRESULT hr = output_data_buffer.pSample->ConvertToContiguousBuffer(
      media_buffer.Receive());
  if (FAILED(hr)) {
    return nullptr;
  }

  uint8_t* data = nullptr;
  DWORD data_size = 0;
  hr = media_buffer->Lock(&data, nullptr, &data_size);
  if (FAILED(hr)) {
    return nullptr;
  }

  LONGLONG sample_time = 0;
  hr = output_data_buffer.pSample->GetSampleTime(&sample_time);
  if (FAILED(hr))
    return nullptr;
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
                          ChannelLayoutToChannelCount(config_.channel_layout());

  // The timestamp will be calculated by |discard_helper_| later on.
  return AudioBuffer::CopyFrom(
      ConvertToSampleFormat(output_sample_size_), config_.channel_layout(),
      ChannelLayoutToChannelCount(config_.channel_layout()),
      config_.samples_per_second(), frame_count, &data, kNoTimestamp());
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
  stride = ((stride + 15) & ~15);  // Stride has to be divisible by 16.
  if (FAILED(hr)) {
    DVLOG(1) << "Failed to obtain stride.";
    return nullptr;
  }

  // Number of rows has to be divisible by 16.
  LONG rows = ((config_.coded_size().height() + 15) & ~15);

  scoped_refptr<VideoFrame> frame = VideoFrame::WrapExternalYuvData(
      PIXEL_FORMAT_YV12, config_.coded_size(), config_.visible_rect(),
      config_.natural_size(), stride, stride / 2, stride / 2,
      const_cast<uint8_t*>(data_buffer->data()),
      const_cast<uint8_t*>(data_buffer->data() +
                           (rows * stride + rows * stride / 4)),
      const_cast<uint8_t*>(data_buffer->data() + (rows * stride)), timestamp);
  frame->AddDestructionObserver(base::Bind(&BufferHolder, data_buffer));
  return frame;
}

template <DemuxerStream::Type StreamType>
base::win::ScopedComPtr<IMFSample> WMFDecoderImpl<StreamType>::CreateSample(
    DWORD buffer_size,
    int buffer_alignment) const {
  base::win::ScopedComPtr<IMFSample> sample;
  HRESULT hr = MFCreateSample(sample.Receive());
  if (FAILED(hr))
    return base::win::ScopedComPtr<IMFSample>();

  base::win::ScopedComPtr<IMFMediaBuffer> buffer;
  hr = MFCreateAlignedMemoryBuffer(buffer_size, buffer_alignment,
                                   buffer.Receive());
  if (FAILED(hr))
    return base::win::ScopedComPtr<IMFSample>();

  hr = sample->AddBuffer(buffer.get());
  if (FAILED(hr))
    return base::win::ScopedComPtr<IMFSample>();

  return sample;
}

template <>
void WMFDecoderImpl<DemuxerStream::AUDIO>::ResetTimestampState() {
  discard_helper_.reset(new AudioDiscardHelper(config_.samples_per_second(),
                                               config_.codec_delay()));
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
