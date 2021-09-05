// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#include "platform_media/gpu/pipeline/win/wmf_media_pipeline.h"

#include <mfidl.h>
#include <d3d9.h>  // if included before |mfidl.h| breaks <propvarutil.h>
#include <dxva2api.h>  // if included before |mfidl.h| breaks <propvarutil.h>

#include <mfreadwrite.h>
#include <wrl/client.h>
#include <propvarutil.h>
#include <Mferror.h>

#include <algorithm>
#include <string>
#include <memory>
#include <unordered_map>

#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/stl_util.h"
#include "base/task_runner_util.h"
#include "base/task/task_traits.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/data_buffer.h"
#include "media/base/data_source.h"
#include "media/base/timestamp_constants.h"

#include "platform_media/common/platform_logging_util.h"
#include "platform_media/common/platform_mime_util.h"
#include "platform_media/common/win/mf_util.h"
#include "platform_media/gpu/data_source/ipc_data_source.h"
#include "platform_media/gpu/decoders/win/wmf_byte_stream.h"

namespace media {

namespace {

const int kMicrosecondsPerSecond = 1000000;
const int kHundredsOfNanosecondsPerSecond = 10000000;

}  // namespace

class WMFMediaPipeline::AudioTimestampCalculator {
 public:
  AudioTimestampCalculator();
  ~AudioTimestampCalculator() {}

  void SetChannelCount(int channel_count);
  void SetBytesPerSample(int bytes_per_sample);
  void SetSamplesPerSecond(int samples_per_second);
  void RecapturePosition();

  int64_t GetFramesCount(int64_t data_size);
  base::TimeDelta GetTimestamp(int64_t timestamp_hns, bool discontinuity);
  base::TimeDelta GetDuration(int64_t frames_count);
  void UpdateFrameCounter(int64_t frames_count);

 private:
  int channel_count_;
  int bytes_per_sample_;
  int samples_per_second_;
  int64_t frame_sum_;
  int64_t frame_offset_;
  bool must_recapture_position_;
};  // class WMFMediaPipeline::AudioTimestampCalculator

class WMFMediaPipeline::ThreadedImpl {
  public:
  ThreadedImpl();
  ~ThreadedImpl();

  void Initialize(ipc_data_source::Info ipc_source_info,
                  InitializeCB initialize_cb);
  void ReadData(IPCDecodingBuffer buffer);
  void Seek(base::TimeDelta time, SeekCB seek_cb);

  private:
  bool CreateSourceReaderCallbackAndAttributes(
      const std::string& mime_type,
      Microsoft::WRL::ComPtr<IMFAttributes>* attributes);

  bool RetrieveStreamIndices();
  bool ConfigureStream(DWORD stream_index);
  bool ConfigureSourceReader();
  bool HasMediaStream(PlatformMediaDataType type) const;
  void SetNoMediaStream(PlatformMediaDataType type);
  base::TimeDelta GetDuration();
  int GetBitrate(base::TimeDelta duration);
  bool GetStride(int* stride);
  bool GetAudioDecoderConfig(PlatformAudioConfig* audio_config);
  bool GetVideoDecoderConfig(PlatformVideoConfig* video_config);
  void OnReadSample(MediaDataStatus status,
                    DWORD stream_index,
                    const Microsoft::WRL::ComPtr<IMFSample>& sample);
  bool CreateDataBuffer(IMFSample* sample,
                        IPCDecodingBuffer* decoding_buffer);
  bool CreateDataBufferFromMemory(IMFSample* sample,
                                  IPCDecodingBuffer* decoding_buffer);
  bool GetSourceInt32Attribute(REFGUID attr, int& result);
  bool GetSourceInt64Attribute(REFGUID attr, int64_t& result);

  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  bool is_streaming_ = false;
  Microsoft::WRL::ComPtr<IMFSourceReader> source_reader_;

  DWORD stream_indices_[kPlatformMediaDataTypeCount];

  std::unique_ptr<AudioTimestampCalculator> audio_timestamp_calculator_;

  IPCDecodingBuffer ipc_decoding_buffers_[kPlatformMediaDataTypeCount];

  // See |WMFDecoderImpl::get_stride_function_|.
  decltype(MFGetStrideForBitmapInfoHeader)* get_stride_function_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<ThreadedImpl> weak_ptr_factory_;
};

namespace {

struct AutoPropVariant {
  AutoPropVariant() {
    PropVariantInit(&var);
  }

  ~AutoPropVariant() {
    PropVariantClear(&var);
  }

  PROPVARIANT var;
};

class SourceReaderCallback : public IMFSourceReaderCallback {
 public:
  using OnReadSampleCB =
      base::Callback<void(MediaDataStatus status,
                          DWORD stream_index,
                          const Microsoft::WRL::ComPtr<IMFSample>& sample)>;

  explicit SourceReaderCallback(const OnReadSampleCB& on_read_sample_cb);

  // IUnknown methods
  STDMETHODIMP QueryInterface(REFIID iid, void** ppv) override;
  STDMETHODIMP_(ULONG) AddRef() override;
  STDMETHODIMP_(ULONG) Release() override;

  // IMFSourceReaderCallback methods
  STDMETHODIMP OnReadSample(HRESULT status,
                            DWORD stream_index,
                            DWORD stream_flags,
                            LONGLONG timestamp_hns,
                            IMFSample* unwrapped_sample) override;
  STDMETHODIMP OnEvent(DWORD, IMFMediaEvent*) override;
  STDMETHODIMP OnFlush(DWORD) override;

 private:
  // Destructor is private. Caller should call Release.
  virtual ~SourceReaderCallback() {}

  OnReadSampleCB on_read_sample_cb_;
  LONG reference_count_;

  DISALLOW_COPY_AND_ASSIGN(SourceReaderCallback);
};  // class SourceReaderCallback

SourceReaderCallback::SourceReaderCallback(
    const OnReadSampleCB& on_read_sample_cb)
    : on_read_sample_cb_(on_read_sample_cb), reference_count_(1) {
  DCHECK(!on_read_sample_cb.is_null());
}

STDMETHODIMP SourceReaderCallback::QueryInterface(REFIID iid, void** ppv) {
  static const QITAB qit[] = {
      QITABENT(SourceReaderCallback, IMFSourceReaderCallback), {0},
  };
  return QISearch(this, qit, iid, ppv);
}

STDMETHODIMP_(ULONG) SourceReaderCallback::AddRef() {
  return InterlockedIncrement(&reference_count_);
}

STDMETHODIMP_(ULONG) SourceReaderCallback::Release() {
  ULONG uCount = InterlockedDecrement(&reference_count_);
  if (uCount == 0) {
    delete this;
  }
  return uCount;
}

STDMETHODIMP SourceReaderCallback::OnReadSample(HRESULT status,
                                                DWORD stream_index,
                                                DWORD stream_flags,
                                                LONGLONG timestamp_hns,
                                                IMFSample* unwrapped_sample) {
  Microsoft::WRL::ComPtr<IMFSample> sample(unwrapped_sample);

  if (FAILED(status)) {
    LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__ << ": Error";
    on_read_sample_cb_.Run(MediaDataStatus::kMediaError, stream_index,
                           sample);
    return S_OK;
  }

  if (stream_flags & MF_SOURCE_READERF_ENDOFSTREAM) {
    VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__ << ": EndOfStream";
    on_read_sample_cb_.Run(MediaDataStatus::kEOS, stream_index, sample);
    return S_OK;
  }

  if (stream_flags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED) {
    VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__ << ": MediaTypeChanged";
    on_read_sample_cb_.Run(MediaDataStatus::kConfigChanged, stream_index,
                           sample);
    return S_OK;
  }

  if (!sample) {
    // NULL |sample| can occur when there is a gap in the stream what
    // is signalled by |MF_SOURCE_READERF_STREAMTICK| flag but from the sparse
    // documentation that can be found on the subject it seems to be used only
    // with "live sources" of AV data (like cameras?), so we should be safe to
    // ignore it
    DCHECK(!(stream_flags & MF_SOURCE_READERF_STREAMTICK));
    LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__ << ": Abort";
    on_read_sample_cb_.Run(MediaDataStatus::kMediaError, stream_index,
                           sample);
    return E_ABORT;
  }

  VLOG(7) << " PROPMEDIA(GPU) : " << __FUNCTION__ << ": Deliver Sample";
  on_read_sample_cb_.Run(MediaDataStatus::kOk, stream_index, sample);
  return S_OK;
}

STDMETHODIMP SourceReaderCallback::OnEvent(DWORD, IMFMediaEvent*) {
  return S_OK;
}

STDMETHODIMP SourceReaderCallback::OnFlush(DWORD) {
  return S_OK;
}

// Helper function that counts how many bits are set in the input number.
int NumberOfSetBits(uint32_t i) {
  int number_of_set_bits = 0;
  while (i > 0) {
    if (i & 1) {
      number_of_set_bits++;
    }
    i = i >> 1;
  }
  return number_of_set_bits;
}

}  // namespace

WMFMediaPipeline::AudioTimestampCalculator::AudioTimestampCalculator()
    : channel_count_(0),
      bytes_per_sample_(0),
      samples_per_second_(0),
      frame_sum_(0),
      frame_offset_(0),
      must_recapture_position_(false) {
}

void WMFMediaPipeline::AudioTimestampCalculator::SetChannelCount(
    int channel_count) {
  channel_count_ = channel_count;
}

void WMFMediaPipeline::AudioTimestampCalculator::SetBytesPerSample(
    int bytes_per_sample) {
  bytes_per_sample_ = bytes_per_sample;
}

void WMFMediaPipeline::AudioTimestampCalculator::SetSamplesPerSecond(
    int samples_per_second) {
  samples_per_second_ = samples_per_second;
}

void WMFMediaPipeline::AudioTimestampCalculator::RecapturePosition() {
  must_recapture_position_ = true;
}

int64_t WMFMediaPipeline::AudioTimestampCalculator::GetFramesCount(
    int64_t data_size) {
  return data_size / bytes_per_sample_ / channel_count_;
}

base::TimeDelta WMFMediaPipeline::AudioTimestampCalculator::GetTimestamp(
    int64_t timestamp_hns,
    bool discontinuity) {
  // If this sample block comes after a discontinuity (i.e. a gap or seek)
  // reset the frame counters, and capture the timestamp. Future timestamps
  // will be offset from this block's timestamp.
  if (must_recapture_position_ || discontinuity != 0) {
    frame_sum_ = 0;
    frame_offset_ =
        timestamp_hns * samples_per_second_ / kHundredsOfNanosecondsPerSecond;
    must_recapture_position_ = false;
  }
  return base::TimeDelta::FromMicroseconds((frame_offset_ + frame_sum_) *
                                           kMicrosecondsPerSecond /
                                           samples_per_second_);
}

base::TimeDelta WMFMediaPipeline::AudioTimestampCalculator::GetDuration(
    int64_t frames_count) {
  return base::TimeDelta::FromMicroseconds(
      frames_count * kMicrosecondsPerSecond / samples_per_second_);
}

void WMFMediaPipeline::AudioTimestampCalculator::UpdateFrameCounter(
    int64_t frames_count) {
  frame_sum_ += frames_count;
}

WMFMediaPipeline::WMFMediaPipeline() = default;

WMFMediaPipeline::~WMFMediaPipeline() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // base::Unretained(threaded_impl_) usage in this class methods when posting
  // to the media pipeline worker sequence is safe since the posted tasks will
  // be executed strictly before the following DeleteSoon.
  if (threaded_impl_) {
    DCHECK(media_pipeline_task_runner_);
    media_pipeline_task_runner_->DeleteSoon(FROM_HERE,
                                            threaded_impl_.release());
  }
}

void WMFMediaPipeline::Initialize(ipc_data_source::Info source_info,
                                  InitializeCB initialize_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  LOG(INFO) << " PROPMEDIA(GPU) : " << __FUNCTION__ << " mime_type="
            << source_info.mime_type;

  media_pipeline_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
  threaded_impl_ = std::make_unique<ThreadedImpl>();

  // See comments in the destructor about base::Unretained().
  media_pipeline_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WMFMediaPipeline::ThreadedImpl::Initialize,
                     base::Unretained(threaded_impl_.get()),
                     std::move(source_info), std::move(initialize_cb)));
}

void WMFMediaPipeline::ReadMediaData(IPCDecodingBuffer ipc_decoding_buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // We might have some data ready to send, see comments in
  // WMFMediaPipeline::ThreadedImpl::OnReadSample.
  if (ipc_decoding_buffer.status() == MediaDataStatus::kConfigChanged &&
      ipc_decoding_buffer.data_size() > 0) {
    ipc_decoding_buffer.set_status(MediaDataStatus::kOk);
    IPCDecodingBuffer::Reply(std::move(ipc_decoding_buffer));
    return;
  }

  // See comments in the destructor about base::Unretained().
  media_pipeline_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&WMFMediaPipeline::ThreadedImpl::ReadData,
                                base::Unretained(threaded_impl_.get()),
                                std::move(ipc_decoding_buffer)));
}

void WMFMediaPipeline::Seek(base::TimeDelta time, SeekCB seek_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__ << ": time " << time;

  // See comments in the destructor about base::Unretained().
  media_pipeline_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&WMFMediaPipeline::ThreadedImpl::Seek,
                                base::Unretained(threaded_impl_.get()), time,
                                std::move(seek_cb)));
}

WMFMediaPipeline::ThreadedImpl::ThreadedImpl()
    :  // We are constructing this object on the main thread
      main_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      audio_timestamp_calculator_(new AudioTimestampCalculator),
      get_stride_function_(nullptr),
      weak_ptr_factory_(this) {
  std::fill(std::begin(stream_indices_), std::end(stream_indices_),
            static_cast<DWORD>(MF_SOURCE_READER_INVALID_STREAM_INDEX));
  DETACH_FROM_SEQUENCE(sequence_checker_);
  VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__ << ": threaded_impl=" << this;
}

WMFMediaPipeline::ThreadedImpl::~ThreadedImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__ << ": threaded_impl=" << this;
}

void WMFMediaPipeline::ThreadedImpl::Initialize(
    ipc_data_source::Info ipc_source_info,
    InitializeCB initialize_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!source_reader_);

  is_streaming_ = ipc_source_info.is_streaming;

  bool ok = false;
  PlatformMediaTimeInfo time_info;
  int bitrate = 0;
  PlatformAudioConfig audio_config;
  PlatformVideoConfig video_config;

  do {
    // We've already made this check in WebMediaPlayerImpl, but that's been in
    // a different process, so let's take its result with a grain of salt.
    const bool has_platform_support =
        IsPlatformMediaPipelineAvailable(PlatformMediaCheckType::FULL);

    get_stride_function_ = reinterpret_cast<decltype(get_stride_function_)>(
        GetFunctionFromLibrary("MFGetStrideForBitmapInfoHeader", "evr.dll"));

    if (!has_platform_support || !get_stride_function_) {
      LOG(WARNING) << " PROPMEDIA(GPU) : " << __FUNCTION__
                   << " Can't access required media libraries in the system";
      break;
    }

    Microsoft::WRL::ComPtr<IMFAttributes> source_reader_attributes;
    if (!CreateSourceReaderCallbackAndAttributes(ipc_source_info.mime_type,
                                                 &source_reader_attributes)) {
      LOG(WARNING) << " PROPMEDIA(GPU) : " << __FUNCTION__
                   << " Failed to create source reader attributes";
      break;
    }

    Microsoft::WRL::ComPtr<WMFByteStream> byte_stream(
        Microsoft::WRL::Make<WMFByteStream>());
    byte_stream->Initialize(main_task_runner_,
                            std::move(ipc_source_info.buffer),
                            ipc_source_info.is_streaming, ipc_source_info.size);

    const HRESULT hr = MFCreateSourceReaderFromByteStream(
        byte_stream.Get(), source_reader_attributes.Get(),
        source_reader_.GetAddressOf());
    if (FAILED(hr)) {
      LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__
                 << " Failed to create SOFTWARE source reader.";
      source_reader_.Reset();
      break;
    }

    if (!RetrieveStreamIndices()) {
      LOG(WARNING) << " PROPMEDIA(GPU) : " << __FUNCTION__
                   << " Failed to find streams";
      break;
    }

    if (!ConfigureSourceReader()) {
      LOG(WARNING) << " PROPMEDIA(GPU) : " << __FUNCTION__
                   << " Failed configure source reader";
      break;
    }

    time_info.duration = GetDuration();
    bitrate = GetBitrate(time_info.duration);

    if (HasMediaStream(PlatformMediaDataType::PLATFORM_MEDIA_AUDIO)) {
      if (!GetAudioDecoderConfig(&audio_config)) {
        LOG(WARNING) << " PROPMEDIA(GPU) : " << __FUNCTION__
                     << " Failed to get Audio Decoder Config";
        break;
      }
    }

    if (HasMediaStream(PlatformMediaDataType::PLATFORM_MEDIA_VIDEO)) {
      if (!GetVideoDecoderConfig(&video_config)) {
        LOG(WARNING) << " PROPMEDIA(GPU) : " << __FUNCTION__
                     << " Failed to get Video Decoder Config";
        break;
      }
    }

    VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
            << " FinalizeInitialization successful with PlatformAudioConfig :"
            << Loggable(audio_config);
    VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
            << " FinalizeInitialization successful with PlatformVideoConfig :"
            << Loggable(video_config);
    ok = true;
  } while (false);

  main_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(initialize_cb), ok, bitrate,
                                time_info, audio_config, video_config));
}

void WMFMediaPipeline::ThreadedImpl::ReadData(
    IPCDecodingBuffer ipc_decoding_buffer) {
  PlatformMediaDataType type = ipc_decoding_buffer.type();
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!ipc_decoding_buffers_[type]);

  VLOG(7) << " PROPMEDIA(GPU) : " << __FUNCTION__ << " type=" << type;

  DCHECK(source_reader_);

  HRESULT hr = source_reader_->ReadSample(stream_indices_[type], 0, nullptr,
                                          nullptr, nullptr, nullptr);

  if (FAILED(hr)) {
    LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__
               << " Failed to read audio sample hr=" << hr;
    ipc_decoding_buffer.set_status(MediaDataStatus::kMediaError);
    main_task_runner_->PostTask(FROM_HERE,
                                base::BindOnce(&IPCDecodingBuffer::Reply,
                                               std::move(ipc_decoding_buffer)));
  }
  ipc_decoding_buffers_[type] = std::move(ipc_decoding_buffer);
}

void WMFMediaPipeline::ThreadedImpl::OnReadSample(
    MediaDataStatus status,
    DWORD stream_index,
    const Microsoft::WRL::ComPtr<IMFSample>& sample) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  VLOG(7) << " PROPMEDIA(GPU) : " << __FUNCTION__
          << ", status: " << (int)status;

  PlatformMediaDataType media_type =
      PlatformMediaDataType::PLATFORM_MEDIA_AUDIO;
  if (stream_index ==
      stream_indices_[PlatformMediaDataType::PLATFORM_MEDIA_VIDEO]) {
    media_type = PlatformMediaDataType::PLATFORM_MEDIA_VIDEO;
  } else if (stream_index !=
             stream_indices_[PlatformMediaDataType::PLATFORM_MEDIA_AUDIO]) {
    NOTREACHED() << "Unknown stream type";
  }
  DCHECK(ipc_decoding_buffers_[media_type]);
  if (!ipc_decoding_buffers_[media_type])
    return;

  IPCDecodingBuffer ipc_decoding_buffer =
      std::move(ipc_decoding_buffers_[media_type]);
  DCHECK(ipc_decoding_buffer.type() == media_type);
  switch (status) {
    case MediaDataStatus::kOk:
      DCHECK(sample);
      if (!CreateDataBuffer(sample.Get(), &ipc_decoding_buffer)) {
        status = MediaDataStatus::kMediaError;
      }
      break;
    case MediaDataStatus::kEOS:
      break;

    case MediaDataStatus::kMediaError:
      LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__
                 << " status MediaDataStatus::kMediaError";
      break;

    case MediaDataStatus::kConfigChanged:
      // Chromium's pipeline does not want any decoded data together with the
      // config change messages. So we copy the decoded data into the buffer now
      // but send them the next time we will be asked for data, see
      // WMFMediaPipeline::ReadMediaData.
      DCHECK(sample);
      if (!CreateDataBuffer(sample.Get(), &ipc_decoding_buffer)) {
        status = MediaDataStatus::kMediaError;
        break;
      }
      switch (media_type) {
        case PlatformMediaDataType::PLATFORM_MEDIA_AUDIO: {
          PlatformAudioConfig audio_config;
          if (!GetAudioDecoderConfig(&audio_config)) {
            LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__
                       << " Error while getting decoder audio configuration"
                       << " changing status to MediaDataStatus::kMediaError";
            status = MediaDataStatus::kMediaError;
            break;
          }
          VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
                  << Loggable(audio_config);
          ipc_decoding_buffer.GetAudioConfig() = audio_config;
          break;
        }
        case PlatformMediaDataType::PLATFORM_MEDIA_VIDEO: {
          PlatformVideoConfig video_config;
          if (!GetVideoDecoderConfig(&video_config)) {
            LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__
                       << " Error while getting decoder video configuration"
                       << " changing status to MediaDataStatus::kMediaError";
            status = MediaDataStatus::kMediaError;
            break;
          }
          ipc_decoding_buffer.GetVideoConfig() = video_config;
          break;
        }
      }
      break;
  }

  ipc_decoding_buffer.set_status(status);
  main_task_runner_->PostTask(FROM_HERE,
                              base::BindOnce(&IPCDecodingBuffer::Reply,
                                             std::move(ipc_decoding_buffer)));
}

bool WMFMediaPipeline::ThreadedImpl::CreateDataBufferFromMemory(
    IMFSample* sample,
    IPCDecodingBuffer* decoding_buffer) {
  VLOG(7) << " PROPMEDIA(GPU) : " << __FUNCTION__;

  // Get a pointer to the IMFMediaBuffer in the sample.
  Microsoft::WRL::ComPtr<IMFMediaBuffer> output_buffer;
  HRESULT hr = sample->ConvertToContiguousBuffer(output_buffer.GetAddressOf());
  if (FAILED(hr)) {
    LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__
               << " Failed to get pointer to data in sample.";
    return false;
  }

  // Get the actual data from the IMFMediaBuffer
  uint8_t* data = nullptr;
  DWORD data_size = 0;
  hr = output_buffer->Lock(&data, NULL, &data_size);
  if (FAILED(hr)) {
    LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__
               << " Failed to lock buffer.";
    return false;
  }
  uint8_t* memory = decoding_buffer->PrepareMemory(data_size);
  if (memory) {
    memcpy(memory, data, data_size);
  }

  // Unlock the IMFMediaBuffer buffer.
  output_buffer->Unlock();

  return memory != nullptr;
}

bool WMFMediaPipeline::ThreadedImpl::CreateDataBuffer(
    IMFSample* sample,
    IPCDecodingBuffer* decoding_buffer) {
  VLOG(7) << " PROPMEDIA(GPU) : " << __FUNCTION__;

  if (!CreateDataBufferFromMemory(sample, decoding_buffer))
    return false;

  int64_t timestamp_hns;  // timestamp in hundreds of nanoseconds
  HRESULT hr = sample->GetSampleTime(&timestamp_hns);
  if (FAILED(hr)) {
    timestamp_hns = 0;
  }

  int64_t duration_hns;  // duration in hundreds of nanoseconds
  hr = sample->GetSampleDuration(&duration_hns);
  if (FAILED(hr)) {
    duration_hns = 0;
  }

  UINT32 discontinuity;
  hr = sample->GetUINT32(MFSampleExtension_Discontinuity, &discontinuity);
  if (FAILED(hr)) {
    discontinuity = 0;
  }

  switch (decoding_buffer->type()) {
    case PlatformMediaDataType::PLATFORM_MEDIA_AUDIO: {
      // We calculate the timestamp and the duration based on the number of
      // audio frames we've already played. We don't trust the timestamp
      // stored on the IMFSample, as sometimes it's wrong, possibly due to
      // buggy encoders?
      decoding_buffer->set_timestamp(audio_timestamp_calculator_->GetTimestamp(
          timestamp_hns, discontinuity != 0));
      int64_t frames_count = audio_timestamp_calculator_->GetFramesCount(
          static_cast<int64_t>(decoding_buffer->data_size()));
      decoding_buffer->set_duration(
          audio_timestamp_calculator_->GetDuration(frames_count));
      audio_timestamp_calculator_->UpdateFrameCounter(frames_count);
      break;
    }
    case PlatformMediaDataType::PLATFORM_MEDIA_VIDEO: {
      decoding_buffer->set_timestamp(
          base::TimeDelta::FromMicroseconds(timestamp_hns / 10));
      decoding_buffer->set_duration(
          base::TimeDelta::FromMicroseconds(duration_hns / 10));
      break;
    }
  }

  return true;
}

void WMFMediaPipeline::ThreadedImpl::Seek(base::TimeDelta time, SeekCB seek_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  bool success = false;
  do {
    // Seek requests on a streaming data source can confuse WMF.
    // Chromium sometimes seeks to the beginning of a stream when starting up.
    // Since that should be a no-op, we just pretend it succeeded.
    if (is_streaming_ && time == base::TimeDelta()) {
      success = true;
      break;
    }

    AutoPropVariant position;
    // IMFSourceReader::SetCurrentPosition expects position in 100-nanosecond
    // units, so we have to multiply time in microseconds by 10.
    HRESULT hr =
        InitPropVariantFromInt64(time.InMicroseconds() * 10, &position.var);
    if (FAILED(hr))
      break;

    audio_timestamp_calculator_->RecapturePosition();

    hr = source_reader_->SetCurrentPosition(GUID_NULL, position.var);
    if (FAILED(hr)) {
      VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
              << " : SetCurrentPosition error hr=" << hr;
      break;
    }

    success = true;
  } while (false);

  main_task_runner_->PostTask(FROM_HERE,
                              base::BindOnce(std::move(seek_cb), success));
}

bool WMFMediaPipeline::ThreadedImpl::HasMediaStream(
    PlatformMediaDataType type) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return stream_indices_[type] !=
      static_cast<DWORD>(MF_SOURCE_READER_INVALID_STREAM_INDEX);
}

void WMFMediaPipeline::ThreadedImpl::SetNoMediaStream(
    PlatformMediaDataType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  stream_indices_[type] =
      static_cast<DWORD>(MF_SOURCE_READER_INVALID_STREAM_INDEX);
}

bool WMFMediaPipeline::ThreadedImpl::GetAudioDecoderConfig(
    PlatformAudioConfig* audio_config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(source_reader_);

  // In case of some audio streams SourceReader might not get everything
  // right just from examining the stream (i.e. during initialization), so some
  // of the values reported here might be wrong. In such case first sample
  // shall be decoded with |MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED| status,
  // what will allow us to get proper configuration.

  audio_config->format = SampleFormat::kSampleFormatF32;

  Microsoft::WRL::ComPtr<IMFMediaType> media_type;
  HRESULT hr = source_reader_->GetCurrentMediaType(
      stream_indices_[PlatformMediaDataType::PLATFORM_MEDIA_AUDIO],
      media_type.GetAddressOf());

  if (FAILED(hr) || !media_type) {
    LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__
               << " Failed to obtain media type hr=" << hr;
    return false;
  }

  audio_config->channel_count =
      MFGetAttributeUINT32(media_type.Get(), MF_MT_AUDIO_NUM_CHANNELS, 0);
  if (audio_config->channel_count == 0) {
    audio_config->channel_count = NumberOfSetBits(
        MFGetAttributeUINT32(media_type.Get(), MF_MT_AUDIO_CHANNEL_MASK, 0));
  }

  audio_timestamp_calculator_->SetChannelCount(audio_config->channel_count);

  audio_timestamp_calculator_->SetBytesPerSample(
      MFGetAttributeUINT32(media_type.Get(), MF_MT_AUDIO_BITS_PER_SAMPLE, 16) /
      8);

  audio_config->samples_per_second =
      MFGetAttributeUINT32(media_type.Get(), MF_MT_AUDIO_SAMPLES_PER_SECOND, 0);
  audio_timestamp_calculator_->SetSamplesPerSecond(
      audio_config->samples_per_second);

  VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
          << " Created a PlatformAudioConfig from IMFMediaType attributes :"
          << Loggable(*audio_config);

  return true;
}

bool WMFMediaPipeline::ThreadedImpl::GetVideoDecoderConfig(
    PlatformVideoConfig* video_config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(source_reader_);

  // In case of some video streams SourceReader might not get everything
  // right just from examining the stream (i.e. during initialization), so some
  // of the values reported here might be wrong. In such case first sample
  // shall be decoded with |MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED| status,
  // what will allow us to get proper configuration.

  Microsoft::WRL::ComPtr<IMFMediaType> media_type;
  HRESULT hr = source_reader_->GetCurrentMediaType(
      stream_indices_[PlatformMediaDataType::PLATFORM_MEDIA_VIDEO],
      media_type.GetAddressOf());

  if (FAILED(hr)) {
    LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__
               << " Failed to obtain media type hr=" << hr;
    return false;
  }

  uint32_t frame_width = 0;
  uint32_t frame_height = 0;
  hr = MFGetAttributeSize(media_type.Get(), MF_MT_FRAME_SIZE, &frame_width,
                          &frame_height);
  if (FAILED(hr)) {
    LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__
               << " Failed to obtain width and height.";
    return false;
  }

  video_config->coded_size = gfx::Size(frame_width, frame_height);

  // The visible rect and the natural size of the video frame have to be
  // calculated with consideration of the pan scan aperture, the display
  // aperture and the pixel aspec ratio. For more info see:
  // http://msdn.microsoft.com/en-us/library/windows/desktop/bb530115(v=vs.85).aspx

  MFVideoArea video_area;
  uint32_t pan_scan_enabled =
      MFGetAttributeUINT32(media_type.Get(), MF_MT_PAN_SCAN_ENABLED, FALSE);
  if (pan_scan_enabled) {
    hr = media_type->GetBlob(MF_MT_PAN_SCAN_APERTURE,
                             reinterpret_cast<uint8_t*>(&video_area),
                             sizeof(MFVideoArea),
                             NULL);
    if (SUCCEEDED(hr)) {
      // MFOffset structure consists of the integer part and the fractional
      // part, but pixels are not divisible, so we ignore the fractional part.
      video_config->visible_rect = gfx::Rect(video_area.OffsetX.value,
                                             video_area.OffsetY.value,
                                             video_area.Area.cx,
                                             video_area.Area.cy);
    }
  }

  if (!pan_scan_enabled || FAILED(hr)) {
    hr = media_type->GetBlob(MF_MT_MINIMUM_DISPLAY_APERTURE,
                             reinterpret_cast<uint8_t*>(&video_area),
                             sizeof(MFVideoArea),
                             NULL);
    if (FAILED(hr)) {
      hr = media_type->GetBlob(MF_MT_GEOMETRIC_APERTURE,
                               reinterpret_cast<uint8_t*>(&video_area),
                               sizeof(MFVideoArea),
                               NULL);
    }

    if (SUCCEEDED(hr)) {
      // MFOffset structure consists of the integer part and the fractional
      // part, but pixels are not divisible, so we ignore the fractional part.
      video_config->visible_rect = gfx::Rect(video_area.OffsetX.value,
                                             video_area.OffsetY.value,
                                             video_area.Area.cx,
                                             video_area.Area.cy);
    } else {
      video_config->visible_rect = gfx::Rect(frame_width, frame_height);
    }
  }

  uint32_t aspect_numerator = 0;
  uint32_t aspect_denominator = 0;
  hr = MFGetAttributeRatio(media_type.Get(), MF_MT_PIXEL_ASPECT_RATIO,
                           &aspect_numerator, &aspect_denominator);
  if (FAILED(hr)) {
    LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__
               << " Failed to obtain pixel aspect ratio.";
    return false;
  }

  if (aspect_numerator == aspect_denominator) {
    video_config->natural_size = gfx::Size(frame_width, frame_height);
  } else if (aspect_numerator > aspect_denominator) {
    video_config->natural_size =
        gfx::Size(MulDiv(frame_width, aspect_numerator, aspect_denominator),
                  frame_height);
  } else {
    video_config->natural_size =
        gfx::Size(frame_width,
                  MulDiv(frame_height, aspect_denominator, aspect_numerator));
  }

  int stride = -1;
  if (!GetStride(&stride))
    return false;

  video_config->planes[VideoFrame::kYPlane].stride = stride;
  video_config->planes[VideoFrame::kVPlane].stride = stride / 2;
  video_config->planes[VideoFrame::kUPlane].stride = stride / 2;

  int rows = frame_height;

  // Y plane is first and is not downsampled.
  video_config->planes[VideoFrame::kYPlane].offset = 0;
  video_config->planes[VideoFrame::kYPlane].size =
      rows * video_config->planes[VideoFrame::kYPlane].stride;

  // In YV12 V and U planes are downsampled vertically and horizontally by 2.
  rows /= 2;

  // V plane preceeds U.
  video_config->planes[VideoFrame::kVPlane].offset =
      video_config->planes[VideoFrame::kYPlane].offset +
      video_config->planes[VideoFrame::kYPlane].size;
  video_config->planes[VideoFrame::kVPlane].size =
      rows * video_config->planes[VideoFrame::kVPlane].stride;

  video_config->planes[VideoFrame::kUPlane].offset =
      video_config->planes[VideoFrame::kVPlane].offset +
      video_config->planes[VideoFrame::kVPlane].size;
  video_config->planes[VideoFrame::kUPlane].size =
      rows * video_config->planes[VideoFrame::kUPlane].stride;

  switch (MFGetAttributeUINT32(media_type.Get(), MF_MT_VIDEO_ROTATION,
                               MFVideoRotationFormat_0)) {
    case MFVideoRotationFormat_90:
      video_config->rotation = VideoRotation::VIDEO_ROTATION_90;
      break;
    case MFVideoRotationFormat_180:
      video_config->rotation = VideoRotation::VIDEO_ROTATION_180;
      break;
    case MFVideoRotationFormat_270:
      video_config->rotation = VideoRotation::VIDEO_ROTATION_270;
      break;
    default:
      video_config->rotation = VideoRotation::VIDEO_ROTATION_0;
      break;
  }
  return true;
}

bool WMFMediaPipeline::ThreadedImpl::CreateSourceReaderCallbackAndAttributes(
    const std::string& mime_type, Microsoft::WRL::ComPtr<IMFAttributes>* attributes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  VLOG(7) << " PROPMEDIA(GPU) : " << __FUNCTION__;

  Microsoft::WRL::ComPtr<IMFSourceReaderCallback> source_reader_callback;
  source_reader_callback = new SourceReaderCallback(BindToCurrentLoop(
      base::Bind(&WMFMediaPipeline::ThreadedImpl::OnReadSample,
                 weak_ptr_factory_.GetWeakPtr())));

  HRESULT hr = MFCreateAttributes((*attributes).GetAddressOf(), 1);
  if (FAILED(hr))
    return false;

  hr = (*attributes)
           ->SetUnknown(MF_SOURCE_READER_ASYNC_CALLBACK,
                        source_reader_callback.Get());
  if (FAILED(hr))
    return false;

  std::wstring mime_type_wstr(mime_type.begin(), mime_type.end());
  hr = (*attributes)
           ->SetString(MF_BYTESTREAM_CONTENT_TYPE, mime_type_wstr.c_str());
  if (FAILED(hr))
    return false;

  return true;
}

bool WMFMediaPipeline::ThreadedImpl::RetrieveStreamIndices() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(source_reader_);

  DWORD stream_index = 0;

  while (!(HasMediaStream(PlatformMediaDataType::PLATFORM_MEDIA_AUDIO) &&
           HasMediaStream(PlatformMediaDataType::PLATFORM_MEDIA_VIDEO))) {
    Microsoft::WRL::ComPtr<IMFMediaType> media_type;
    HRESULT hr = source_reader_->GetNativeMediaType(stream_index, 0,
                                                    media_type.GetAddressOf());

    if (hr == MF_E_INVALIDSTREAMNUMBER)
      break;  // No more streams.

    if (FAILED(hr)) {
      VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
              << " : GetNativeMediaType error hr=" << hr;
    } else {
      GUID major_type;
      hr = media_type->GetMajorType(&major_type);
      if (SUCCEEDED(hr)) {
        if (major_type == MFMediaType_Audio &&
            stream_indices_[PlatformMediaDataType::PLATFORM_MEDIA_AUDIO] ==
                static_cast<DWORD>(MF_SOURCE_READER_INVALID_STREAM_INDEX)) {
          stream_indices_[PlatformMediaDataType::PLATFORM_MEDIA_AUDIO] =
              stream_index;
        } else if (major_type == MFMediaType_Video &&
                   stream_indices_
                           [PlatformMediaDataType::PLATFORM_MEDIA_VIDEO] ==
                       static_cast<DWORD>(
                           MF_SOURCE_READER_INVALID_STREAM_INDEX)) {
          stream_indices_[PlatformMediaDataType::PLATFORM_MEDIA_VIDEO] =
              stream_index;
        }
      }
    }
    ++stream_index;
  }

  return HasMediaStream(PlatformMediaDataType::PLATFORM_MEDIA_AUDIO) ||
         HasMediaStream(PlatformMediaDataType::PLATFORM_MEDIA_VIDEO);
}

bool WMFMediaPipeline::ThreadedImpl::ConfigureStream(DWORD stream_index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(source_reader_);

  DCHECK(stream_index == stream_indices_[PlatformMediaDataType::PLATFORM_MEDIA_AUDIO] ||
         stream_index == stream_indices_[PlatformMediaDataType::PLATFORM_MEDIA_VIDEO]);
  bool is_video = stream_index == stream_indices_[PlatformMediaDataType::PLATFORM_MEDIA_VIDEO];

  Microsoft::WRL::ComPtr<IMFMediaType> new_current_media_type;
  HRESULT hr = MFCreateMediaType(new_current_media_type.GetAddressOf());
  if (FAILED(hr)) {
    LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__
               << " Failed to create media type.";
    return false;
  }

  hr = new_current_media_type->SetGUID(
      MF_MT_MAJOR_TYPE, is_video ? MFMediaType_Video : MFMediaType_Audio);
  if (FAILED(hr)) {
    LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__
               << " Failed to set media major type.";
    return false;
  }

  hr = new_current_media_type->SetGUID(
      MF_MT_SUBTYPE,
      is_video ? MFVideoFormat_YV12 : MFAudioFormat_Float);
  if (FAILED(hr)) {
    LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__
               << " Failed to set media subtype hr=" << hr;
    return false;
  }

  hr = source_reader_->SetCurrentMediaType(
    stream_index, NULL, new_current_media_type.Get());
  if (FAILED(hr)) {
    LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__
               << " Failed to set media type hr=" << hr << ". No "
               << (is_video ? "video" : "audio") << " track?";
    return false;
  }

  // When we set the media type without providing complete media information
  // WMF tries to figure it out on its own.  But it doesn't do it until it's
  // needed -- e.g., when decoding is requested.  Since this figuring-out
  // process can fail, let's force it now by calling GetCurrentMediaType().
  Microsoft::WRL::ComPtr<IMFMediaType> media_type;
  hr = source_reader_->GetCurrentMediaType(stream_index,
                                           media_type.GetAddressOf());
  if (FAILED(hr)) {
    LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__
               << " Failed to obtain media type hr=" << hr;
    return false;
  }

  return true;
}

bool WMFMediaPipeline::ThreadedImpl::ConfigureSourceReader() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(source_reader_);

  static const PlatformMediaDataType media_types[] = {
      PlatformMediaDataType::PLATFORM_MEDIA_AUDIO,
      PlatformMediaDataType::PLATFORM_MEDIA_VIDEO};
  static_assert(base::size(media_types) ==
                    kPlatformMediaDataTypeCount,
                "Not all media types chosen to be configured.");

  bool status = false;
  for (const auto& media_type : media_types) {
    if (!ConfigureStream(stream_indices_[media_type])) {
      SetNoMediaStream(media_type);
    } else {
      DCHECK(HasMediaStream(media_type));
      status = true;
    }
  }

  return status;
}

base::TimeDelta WMFMediaPipeline::ThreadedImpl::GetDuration() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(source_reader_);

  int64_t duration_int64 = 0;
  if (!GetSourceInt64Attribute(MF_PD_DURATION, duration_int64)) {
    VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
            << " duration attribute error is_streaming=" << is_streaming_;
    return media::kInfiniteDuration;
  }

  // Have to divide duration64 by ten to convert from
  // hundreds of nanoseconds (WMF style) to microseconds.
  return base::TimeDelta::FromMicroseconds(duration_int64 / 10);
}

int WMFMediaPipeline::ThreadedImpl::GetBitrate(base::TimeDelta duration) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(source_reader_);
  DCHECK_GT(duration.InMicroseconds(), 0);

  // Calculating the media bitrate
  int audio_bitrate = 0;
  if (!GetSourceInt32Attribute(MF_PD_AUDIO_ENCODING_BITRATE, audio_bitrate)) {
    VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
            << " audio bitrate attribute error is_streaming=" << is_streaming_;
  }

  int video_bitrate = 0;
  if (!GetSourceInt32Attribute(MF_PD_VIDEO_ENCODING_BITRATE, video_bitrate)) {
    VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
            << " video bitrate attribute error is_streaming=" << is_streaming_;
  }

  const int bitrate = std::max(audio_bitrate + video_bitrate, 0);
  if (bitrate > 0 || is_streaming_)
    return bitrate;

  // As a fallback calculate the bitrate from file size and duration.
  if (duration.InMicroseconds() > 0) {
    int64_t file_size_in_bytes = 0;
    if (GetSourceInt64Attribute(MF_PD_TOTAL_FILE_SIZE, file_size_in_bytes))
      return (8000000.0 * file_size_in_bytes) / duration.InMicroseconds();
    VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
            << " total file size attribute error";
  }
  LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__
             << " Failed to obtain media bitrate.";

  return 0;
}

bool WMFMediaPipeline::ThreadedImpl::GetStride(int* stride) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(source_reader_);

  Microsoft::WRL::ComPtr<IMFMediaType> media_type;
  HRESULT hr = source_reader_->GetCurrentMediaType(
      stream_indices_[PlatformMediaDataType::PLATFORM_MEDIA_VIDEO],
      media_type.GetAddressOf());

  if (FAILED(hr)) {
    LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__
               << " Failed to obtain media type hr=" << hr;
    return false;
  }

  UINT32 width = 0;
  UINT32 height = 0;
  hr = MFGetAttributeSize(media_type.Get(), MF_MT_FRAME_SIZE, &width, &height);
  if (FAILED(hr)) {
    LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__
               << " Failed to obtain width and height.";
    return false;
  }

  LONG stride_long = 0;
  hr = get_stride_function_(MFVideoFormat_YV12.Data1, width,
                            &stride_long);
  if (FAILED(hr)) {
    LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__
               << " Failed to obtain stride.";
    return false;
  }

  *stride = base::saturated_cast<int>(stride_long);

  return true;
}

namespace {

bool GetSourceAttribute(IMFSourceReader* reader,
                        REFGUID attr,
                        PROPVARIANT& var) {
  HRESULT hr = reader->GetPresentationAttribute(
      static_cast<DWORD>(MF_SOURCE_READER_MEDIASOURCE), attr, &var);
  if (FAILED(hr)) {
    VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
               << " : Received an error hr=" << hr;
    return false;
  }
  return true;
}

}  // namespace

bool WMFMediaPipeline::ThreadedImpl::GetSourceInt32Attribute(REFGUID attr,
                                                             int& result) {
  DCHECK(!result);
  AutoPropVariant var;
  if (!GetSourceAttribute(source_reader_.Get(), attr, var.var))
    return false;
  int i;
  HRESULT hr = PropVariantToInt32(var.var, &i);
  if (FAILED(hr)) {
    VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
            << " : PropVariant error hr=" << hr;
    return false;
  }
  result = i;
  return true;
}

bool WMFMediaPipeline::ThreadedImpl::GetSourceInt64Attribute(REFGUID attr,
                                                             int64_t& result) {
  DCHECK(!result);
  AutoPropVariant var;
  if (!GetSourceAttribute(source_reader_.Get(), attr, var.var))
    return false;
  int64_t i;
  HRESULT hr = PropVariantToInt64(var.var, &i);
  if (FAILED(hr)) {
    VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
            << " : PropVariant error hr=" << hr;
    return false;
  }
  result = i;
  return true;
}

}  // namespace media
