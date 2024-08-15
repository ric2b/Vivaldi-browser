// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2015 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#include "platform_media/decoders/win/wmf_video_decoder.h"

#include <mfapi.h>
#include <mferror.h>
#include <wmcodecdsp.h>

#include "base/logging.h"
#include "media/base/data_buffer.h"
#include "media/base/video_frame.h"

#include "platform_media/decoders/platform_logging_util.h"
#include "platform_media/sandbox/win/platform_media_init.h"

#define LOG_HR_FAIL(hr, message)                                       \
  do {                                                                 \
    VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__ << " Failed " \
            << message << ", hr=0x" << std::hex << hr;                 \
  } while (false)

#define RETURN_ON_HR_FAIL(hr, message, return_value) \
  do {                                               \
    HRESULT _hr_value = (hr);                        \
    if (FAILED(_hr_value)) {                         \
      LOG_HR_FAIL(_hr_value, message);               \
      return (return_value);                         \
    }                                                \
  } while (false)

namespace media {

namespace {

DWORD kDefaultStreamId = 0;

bool IsValidConfig(const VideoDecoderConfig& config) {
  return config.codec() == VideoCodec::kH264 &&
         config.profile() >= VideoCodecProfile::H264PROFILE_MIN &&
         config.profile() <= VideoCodecProfile::H264PROFILE_MAX;
}

Microsoft::WRL::ComPtr<IMFTransform> CreateWMFDecoder(
    const VideoDecoderConfig& config) {
  using DllFunction = decltype(DllGetClassObject);
  static DllFunction* get_class_object = nullptr;
  if (!get_class_object) {
    HMODULE library = platform_media_init::GetWMFLibraryForH264();
    if (!library)
      return nullptr;
    get_class_object = reinterpret_cast<DllFunction*>(
        ::GetProcAddress(library, "DllGetClassObject"));
    if (!get_class_object) {
      VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
              << " Error while retrieving class object getter function.";
      return nullptr;
    }
  }

  Microsoft::WRL::ComPtr<IClassFactory> factory;
  HRESULT hr =
      get_class_object(__uuidof(CMSH264DecoderMFT), __uuidof(IClassFactory),
                       reinterpret_cast<void**>(factory.GetAddressOf()));
  RETURN_ON_HR_FAIL(hr, "DllGetClassObject()", nullptr);

  Microsoft::WRL::ComPtr<IMFTransform> decoder;
  hr =
      factory->CreateInstance(nullptr, __uuidof(IMFTransform),
                              reinterpret_cast<void**>(decoder.GetAddressOf()));
  RETURN_ON_HR_FAIL(hr, "IClassFactory::CreateInstance(wmf_decoder)", nullptr);

  return decoder;
}

Microsoft::WRL::ComPtr<IMFSample> CreateSample(DWORD buffer_size,
                                               DWORD buffer_alignment) {
  Microsoft::WRL::ComPtr<IMFSample> sample;
  HRESULT hr = MFCreateSample(sample.GetAddressOf());
  RETURN_ON_HR_FAIL(hr, "MFCreateSample()", nullptr);

  Microsoft::WRL::ComPtr<IMFMediaBuffer> buffer;
  DWORD alignment_arg = (buffer_alignment != 0) ? buffer_alignment - 1 : 0;
  hr = MFCreateAlignedMemoryBuffer(buffer_size, alignment_arg,
                                   buffer.GetAddressOf());
  RETURN_ON_HR_FAIL(hr, "MFCreateAlignedMemoryBuffer()", nullptr);

  hr = sample->AddBuffer(buffer.Get());
  RETURN_ON_HR_FAIL(hr, "IMFSample::AddBuffer()", nullptr);

  return sample;
}

Microsoft::WRL::ComPtr<IMFSample> PrepareInputSample(const DecoderBuffer& input,
                                                     DWORD buffer_alignment) {
  Microsoft::WRL::ComPtr<IMFSample> sample =
      CreateSample(input.size(), buffer_alignment);
  if (!sample)
    return nullptr;

  Microsoft::WRL::ComPtr<IMFMediaBuffer> buffer;
  HRESULT hr =
      sample->GetBufferByIndex(kDefaultStreamId, buffer.GetAddressOf());
  RETURN_ON_HR_FAIL(hr, "IMFSample::GetBufferByIndex()", nullptr);

  uint8_t* buff_ptr = nullptr;
  hr = buffer->Lock(&buff_ptr, nullptr, nullptr);
  RETURN_ON_HR_FAIL(hr, "IMFMediaBuffer::Lock()", nullptr);

  memcpy(buff_ptr, input.data(), input.size());

  hr = buffer->Unlock();
  RETURN_ON_HR_FAIL(hr, "IMFMediaBuffer::Unlock()", nullptr);

  hr = buffer->SetCurrentLength(input.size());
  RETURN_ON_HR_FAIL(hr, "IMFMediaBuffer::SetCurrentLength()", nullptr);

  // IMFSample's timestamp is expressed in hundreds of nanoseconds.
  hr = sample->SetSampleTime(input.timestamp().InMicroseconds() * 10);
  RETURN_ON_HR_FAIL(hr, "IMFSample::SetSampleTime()", nullptr);

  return sample;
}

// Extract data from sample and reset its buffer to make it ready to receive
// more data.
bool ExtractSampleData(IMFSample* sample, std::vector<uint8_t>& buffer,
                       LONG stride, LONG rows) {
  Microsoft::WRL::ComPtr<IMFMediaBuffer> media_buffer;
  HRESULT hr = sample->ConvertToContiguousBuffer(media_buffer.GetAddressOf());
  RETURN_ON_HR_FAIL(hr, "IMFSample::ConvertToContiguousBuffer()", false);

  uint8_t* data = nullptr;
  DWORD data_size = 0;
  hr = media_buffer->Lock(&data, nullptr, &data_size);
  RETURN_ON_HR_FAIL(hr, "IMFMediaBuffer::Lock()", false);

  // We must call Unlock() on all code paths.

  // VB-101625: If the number of rows in the output from the decoder is smaller
  // than the number of rows specified by the video config (and the buffer
  // therefore is smaller than expected), then we need to recalculate the
  // number of rows to the (more likely) proper number of rows in the frame and
  // copy the contents over a new buffer with correct (larger) dimensions to
  // prevent a crash due to reading past the end of the buffer in
  // WrapExternalYuvData called below.
  //
  // The issue is probably due to incorrect coding of the video header compared
  // to the encoding of the frames.
  //
  // Buffer organization: 1 set of N rows, 2 sets of N rows that are 1/4 stride.
  // If the calculated N is not N % 16 == 0, it will be reduced to the next
  // lower N matching the modulus, but the resulting colors in the video may
  // look a bit off (if this happens, it would be caused by a bug in Windows).
  if (LONG(data_size) < (rows * stride + rows * stride / 2)) {
    LONG real_rows = (2 * (data_size / stride)) / 3;
    real_rows = (real_rows & ~15);  // Make sure the number of rows is
    // divisible by 16, even if that breaks the video
    VLOG(1) << __FUNCTION__ << " Recalibrated rows : " << real_rows;

    data_size = (rows * stride + rows * stride / 2);
    buffer.resize(data_size);

    // Null the buffer to make sure it is clean
    // This only happens for badly encoded videoes, so no need to optimize
    memset(buffer.data(), 0, data_size);
    // Copy each plane of the frame
    memcpy(buffer.data(), data, real_rows * stride);
    memcpy(buffer.data() + (rows * stride),
           data + (real_rows * stride), real_rows * stride / 4);
    memcpy(buffer.data() + (rows * stride) + rows * stride / 4,
           data + (real_rows * stride) + (real_rows * stride / 4),
           real_rows * stride / 4);
  } else {
    buffer.resize(data_size);
    memcpy(buffer.data(), data, data_size);
  }

  hr = media_buffer->Unlock();
  RETURN_ON_HR_FAIL(hr, "IMFMediaBuffer::Unlock()", false);

  // Prepare `buffer` for reuse.
  hr = media_buffer->SetCurrentLength(0);
  RETURN_ON_HR_FAIL(hr, "IMFMediaBuffer::SetCurrentLength()", false);

  return true;
}

// This function is used as |no_longer_needed_cb| of
// VideoFrame::WrapExternalYuvData to make sure we keep reference to
// the buffer object as long as we need it.
void BufferHolder(std::vector<uint8_t> buffer) {
  /* Intentionally empty */
}

scoped_refptr<VideoFrame> CreateOutputFrame(const VideoDecoderConfig& config,
                                            LONG stride,
                                            IMFSample* sample) {
  LONGLONG sample_time = 0;
  HRESULT hr = sample->GetSampleTime(&sample_time);
  RETURN_ON_HR_FAIL(hr, "IMFSample::GetSampleTime()", nullptr);

  // Number of rows has to be divisible by 16.
  LONG rows = config.coded_size().height();
  LONG adjusted_rows = ((rows + 15) & ~15);

  if (rows != adjusted_rows) {
    // patricia@vivaldi.com : I don't know why we do this and it smells fishy
    VLOG(1) << __FUNCTION__ << " Before rows : " << rows;
    rows = adjusted_rows;
    VLOG(1) << __FUNCTION__ << " After rows : " << rows;
  }

  // The sample time in IMFSample is expressed in hundreds of nanoseconds.
  const base::TimeDelta timestamp = base::Microseconds(sample_time / 10);

  std::vector<uint8_t> buffer;
  if (!ExtractSampleData(sample, buffer, stride, rows))
    return nullptr;

  scoped_refptr<VideoFrame> frame = VideoFrame::WrapExternalYuvData(
      VideoPixelFormat::PIXEL_FORMAT_YV12, config.coded_size(),
      config.visible_rect(), config.natural_size(), stride, stride / 2,
      stride / 2, buffer.data(),
      buffer.data() + (rows * stride + rows * stride / 4),
      buffer.data() + (rows * stride), timestamp);

  frame->AddDestructionObserver(
      base::BindOnce(&BufferHolder, std::move(buffer)));

  return frame;
}

}  // namespace

WMFVideoDecoder::WMFVideoDecoder(
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : task_runner_(std::move(task_runner)) {}

WMFVideoDecoder::~WMFVideoDecoder() {}

void WMFVideoDecoder::Initialize(
    const VideoDecoderConfig& config,
    bool low_delay,
    CdmContext* cdm_context,
    InitCB init_cb,
    const OutputCB& output_cb,
    const WaitingCB& waiting_for_decryption_key_cb) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(output_cb);

  if (!IsValidConfig(config)) {
    VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
            << " Media Config not accepted for codec : "
            << GetCodecName(config.codec());
    std::move(init_cb).Run(DecoderStatus::Codes::kUnsupportedConfig);
    return;
  }
  VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
          << " Supported decoder config for codec : " << Loggable(config);

  config_ = config;
  debug_buffer_logger_.Initialize(GetCodecName(config_.codec()));
  output_cb_ = output_cb;

  if (!ConfigureDecoder()) {
    std::move(init_cb).Run(DecoderStatus::Codes::kFailedToCreateDecoder);
    return;
  }

  std::move(init_cb).Run(DecoderStatus::Codes::kOk);
}

void WMFVideoDecoder::Decode(scoped_refptr<DecoderBuffer> input,
                             DecodeCB decode_cb) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  debug_buffer_logger_.Log(*input);

  media::DecoderStatus::Codes status = DecoderStatus::Codes::kOk;
  if (!DoDecode(std::move(input))) {
    status = DecoderStatus::Codes::kPlatformDecodeFailure;
  }
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(std::move(decode_cb), status));
}

void WMFVideoDecoder::Reset(base::OnceClosure closure) {
  VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__;
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  // Transform needs to be reset, skip this and seeking may fail.
  decoder_->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, 0);

  task_runner_->PostTask(FROM_HERE, std::move(closure));
}

VideoDecoderType WMFVideoDecoder::GetDecoderType() const {
  return VideoDecoderType::kVivWMFDecoder;
}

bool WMFVideoDecoder::NeedsBitstreamConversion() const {
  // WMF h264 decoder must receive data with metadata headers or it would not
  // work for many videos.
  return true;
}

bool WMFVideoDecoder::ConfigureDecoder() {
  decoder_ = CreateWMFDecoder(config_);
  if (!decoder_)
    return false;

  if (!SetInputMediaType())
    return false;

  if (!SetOutputMediaType())
    return false;

  // `GetInputStreamInfo()` requires both input and output to be set.
  MFT_INPUT_STREAM_INFO input_stream_info;
  HRESULT hr =
      decoder_->GetInputStreamInfo(kDefaultStreamId, &input_stream_info);
  RETURN_ON_HR_FAIL(hr, "IMFTransform::GetInputStreamInfo()", false);

  input_buffer_alignment_ = input_stream_info.cbAlignment;

  hr = MFGetStrideForBitmapInfoHeader(MFVideoFormat_YV12.Data1,
                                      config_.coded_size().width(), &stride_);
  RETURN_ON_HR_FAIL(hr, "MFGetStrideForBitmapInfoHeader()", false);

  // Stride has to be divisible by 16.
  LONG adjusted_stride = ((stride_ + 15) & ~15);
  if (stride_ != adjusted_stride) {
    // patricia@vivaldi.com : I don't know why we do this and it smells fishy
    VLOG(1) << __FUNCTION__ << " Changing stride from " << stride_ << " to "
            << adjusted_stride;
    stride_ = adjusted_stride;
  }

  return true;
}

bool WMFVideoDecoder::SetInputMediaType() {
  Microsoft::WRL::ComPtr<IMFMediaType> media_type;
  HRESULT hr = MFCreateMediaType(media_type.GetAddressOf());
  RETURN_ON_HR_FAIL(hr, "MFCreateMediaType()", false);

  hr = media_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
  RETURN_ON_HR_FAIL(hr, "IMFMediaType::SetGUID(MF_MT_MAJOR_TYPE)", false);

  hr = media_type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
  RETURN_ON_HR_FAIL(hr, "IMFMediaType::SetGUID(MF_MT_SUBTYPE)", false);

  hr = media_type->SetUINT32(MF_MT_INTERLACE_MODE,
                             MFVideoInterlace_MixedInterlaceOrProgressive);
  RETURN_ON_HR_FAIL(hr, "IMFMediaType::SetUINT32(MF_MT_INTERLACE_MODE)", false);

  hr = MFSetAttributeSize(media_type.Get(), MF_MT_FRAME_SIZE,
                          config_.coded_size().width(),
                          config_.coded_size().height());
  RETURN_ON_HR_FAIL(hr, "MFSetAttributeSize()", false);

  hr = decoder_->SetInputType(kDefaultStreamId, media_type.Get(), /*flags=*/0);
  RETURN_ON_HR_FAIL(hr, "IMFTransform::SetInputType()", false);

  return true;
}

bool WMFVideoDecoder::SetOutputMediaType() {
  VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__;

  Microsoft::WRL::ComPtr<IMFMediaType> out_media_type;

  for (DWORD i = 0;; ++i) {
    HRESULT hr = decoder_->GetOutputAvailableType(
        kDefaultStreamId, i, out_media_type.GetAddressOf());
    RETURN_ON_HR_FAIL(hr, "IMFTransform::GetOutputAvailableType()", false);

    GUID out_subtype = {0};
    hr = out_media_type->GetGUID(MF_MT_SUBTYPE, &out_subtype);
    RETURN_ON_HR_FAIL(hr, "IMFMediaType::GetGUID(MF_MT_SUBTYPE)", false);

    if (out_subtype == MFVideoFormat_YV12)
      break;
    out_media_type.Reset();
  }
  HRESULT hr = decoder_->SetOutputType(kDefaultStreamId, out_media_type.Get(),
                                       /*flags=*/0);
  RETURN_ON_HR_FAIL(hr, "IMFTransform::SetOutputType()", false);

  MFT_OUTPUT_STREAM_INFO output_stream_info = {0};
  hr = decoder_->GetOutputStreamInfo(kDefaultStreamId, &output_stream_info);
  RETURN_ON_HR_FAIL(hr, "IMFTransform::GetOutputStreamInfo()", false);

  output_sample_.Reset();

  const bool decoder_creates_samples =
      (output_stream_info.dwFlags & (MFT_OUTPUT_STREAM_PROVIDES_SAMPLES |
                                     MFT_OUTPUT_STREAM_CAN_PROVIDE_SAMPLES)) !=
      0;
  if (!decoder_creates_samples) {
    output_sample_ =
        CreateSample(output_stream_info.cbSize, output_stream_info.cbAlignment);
    if (!output_sample_)
      return false;
  }

  return true;
}

bool WMFVideoDecoder::DoDecode(scoped_refptr<DecoderBuffer> input) {
  if (input->end_of_stream()) {
    VLOG(3) << " PROPMEDIA(RENDERER) : " << __FUNCTION__ << " (EOS)";

    // Ask the decoder to output any remaining data.
    decoder_->ProcessMessage(MFT_MESSAGE_COMMAND_DRAIN, 0);
  } else {
    VLOG(5) << " PROPMEDIA(RENDERER) : " << __FUNCTION__ << " ("
            << input->timestamp() << ")";

    const Microsoft::WRL::ComPtr<IMFSample> sample =
        PrepareInputSample(*input, input_buffer_alignment_);
    if (!sample) {
      VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
              << " Failed to create input sample.";
      return false;
    }
    HRESULT hr = decoder_->ProcessInput(kDefaultStreamId, sample.Get(), 0);
    DCHECK_NE(MF_E_NOTACCEPTING, hr)
        << "The transform is neither producing output "
           "nor accepting input? This must not happen, see "
           "ProcessOutput() loop below";
    RETURN_ON_HR_FAIL(hr, "IMFTransform::ProcessInput()", false);
  }

  // We must call `decoder_->ProcessOuput()` without sending more input until it
  // tells that more input is necessary. So loop until that.
  bool need_more_input = false;
  do {
    if (!ProcessOutput(need_more_input))
      return false;
  } while (!need_more_input);

  return true;
}

bool WMFVideoDecoder::ProcessOutput(bool& need_more_input) {
  DCHECK(!need_more_input);

  MFT_OUTPUT_DATA_BUFFER output_data_buffer = {0};
  output_data_buffer.pSample = output_sample_.Get();

  DWORD process_output_status = 0;
  HRESULT hr = decoder_->ProcessOutput(kDefaultStreamId, 1, &output_data_buffer,
                                       &process_output_status);
  IMFCollection* events = output_data_buffer.pEvents;
  if (events != nullptr) {
    // Even though we're not interested in events we have to clean them up.
    events->Release();
  }
  Microsoft::WRL::ComPtr<IMFSample> sample_to_release;
  if (!output_sample_) {
    // The decoder allocated the sample, we must release it.
    sample_to_release.Attach(output_data_buffer.pSample);
  }

  switch (hr) {
    case S_OK: {
      scoped_refptr<VideoFrame> frame =
          CreateOutputFrame(config_, stride_, output_data_buffer.pSample);
      if (!frame)
        return false;

      VLOG(5) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
              << "Decoded: dimensions=(" << frame->coded_size().width() << " "
              << frame->coded_size().height() << ") visible=("
              << frame->visible_rect().x() << " " << frame->visible_rect().y()
              << " " << frame->visible_rect().width() << " "
              << frame->visible_rect().height() << ")";

      task_runner_->PostTask(FROM_HERE, base::BindOnce(output_cb_, frame));
      break;
    }
    case MF_E_TRANSFORM_NEED_MORE_INPUT:
      VLOG(5) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
              << " NEED_MORE_INPUT";
      // Need to to get more input data to produce output.
      need_more_input = true;
      break;
    case MF_E_TRANSFORM_STREAM_CHANGE:
      if (!SetOutputMediaType())
        return false;
      break;
    default:
      LOG_HR_FAIL(hr, "IMFTransform::ProcessOutput()");
      return false;
  }

  return true;
}

}  // namespace media
