// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#include "platform_media/gpu/pipeline/win/wmf_media_pipeline.h"

#include <Mferror.h>
#include <algorithm>
#include <string>

#include "base/callback_helpers.h"
#include "base/numerics/safe_conversions.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/data_buffer.h"
#include "media/base/data_source.h"
#include "media/base/timestamp_constants.h"
#include "media/base/win/mf_initializer.h"

#include "platform_media/common/platform_logging_util.h"
#include "platform_media/common/platform_mime_util.h"
#include "platform_media/common/win/mf_util.h"

using namespace platform_media;

namespace media {

namespace {

const int kMicrosecondsPerSecond = 1000000;
const int kHundredsOfNanosecondsPerSecond = 10000000;

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

WMFMediaPipeline::WMFMediaPipeline(
    DataSource* data_source,
    const AudioConfigChangedCB& audio_config_changed_cb,
    const VideoConfigChangedCB& video_config_changed_cb)
    : audio_config_changed_cb_(audio_config_changed_cb),
      video_config_changed_cb_(video_config_changed_cb),
      byte_stream_(new WMFByteStream(data_source)),
      threaded_impl_(std::make_unique<ThreadedImpl>(data_source)),
      media_pipeline_thread_("media_pipeline_thread"),
      media_pipeline_task_runner_(media_pipeline_thread_.task_runner()),
      weak_ptr_factory_(this) {
  DCHECK(!audio_config_changed_cb_.is_null());
  DCHECK(!video_config_changed_cb_.is_null());
}

WMFMediaPipeline::~WMFMediaPipeline() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  media_pipeline_task_runner_->DeleteSoon(FROM_HERE, threaded_impl_.release());
  if (byte_stream_)
    byte_stream_->Stop();
};

void WMFMediaPipeline::Initialize(const std::string& mime_type,
                                  const InitializeCB& initialize_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  LOG(INFO) << " PROPMEDIA(GPU) : " << __FUNCTION__ << " mime type "
            << mime_type;

  media_pipeline_thread_.Start();
  media_pipeline_task_runner_ = media_pipeline_thread_.task_runner();

  initialize_cb_ = initialize_cb;

  // We set up the byte_stream to run on the main thread, so that it can receive
  // the result of reads when one or another call into the IMFSourceReader ends
  // up hanging while waiting for the result of a read, therefore avoiding
  // deadlocks.
  if (FAILED(byte_stream_->Initialize(
          std::wstring(mime_type.begin(), mime_type.end()).c_str()))) {
    LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__
               << " Failed to create byte stream.";
    InitializeDone(false, -1, PlatformMediaTimeInfo(), PlatformAudioConfig(),
                   PlatformVideoConfig());
    return;
  }

  media_pipeline_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WMFMediaPipeline::ThreadedImpl::Initialize,
                     base::Unretained(threaded_impl_.get()),
                     weak_ptr_factory_.GetWeakPtr(), byte_stream_, mime_type));
}

void WMFMediaPipeline::InitializeDone(bool success,
                                      int bitrate,
                                      PlatformMediaTimeInfo time_info,
                                      PlatformAudioConfig audio_config,
                                      PlatformVideoConfig video_config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(initialize_cb_);
  base::ResetAndReturn(&initialize_cb_)
      .Run(success, bitrate, time_info, audio_config, video_config);
}

void WMFMediaPipeline::ReadAudioData(const ReadDataCB& read_audio_data_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(read_audio_data_cb_.is_null());
  read_audio_data_cb_ = read_audio_data_cb;

  media_pipeline_task_runner_->PostTask(
      FROM_HERE, base::Bind(&WMFMediaPipeline::ThreadedImpl::ReadData,
                            base::Unretained(threaded_impl_.get()),
                            PlatformMediaDataType::PLATFORM_MEDIA_AUDIO));
}

void WMFMediaPipeline::ReadVideoData(const ReadDataCB& read_video_data_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(read_video_data_cb_.is_null());
  read_video_data_cb_ = read_video_data_cb;

  media_pipeline_task_runner_->PostTask(
      FROM_HERE, base::Bind(&WMFMediaPipeline::ThreadedImpl::ReadData,
                            base::Unretained(threaded_impl_.get()),
                            PlatformMediaDataType::PLATFORM_MEDIA_VIDEO));
}

void WMFMediaPipeline::ReadDataDone(PlatformMediaDataType type,
                                    scoped_refptr<DataBuffer> decoded_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  switch (type) {
    case PlatformMediaDataType::PLATFORM_MEDIA_AUDIO:
      DCHECK(read_audio_data_cb_);
      base::ResetAndReturn(&read_audio_data_cb_).Run(decoded_data);
      return;
    case PlatformMediaDataType::PLATFORM_MEDIA_VIDEO:
      DCHECK(read_video_data_cb_);
      base::ResetAndReturn(&read_video_data_cb_).Run(decoded_data);
      return;
    default:
      NOTREACHED() << "Unknown stream type";
  }
}

void WMFMediaPipeline::Seek(base::TimeDelta time, const SeekCB& seek_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__ << ": time " << time;

  seek_cb_ = seek_cb;
  media_pipeline_task_runner_->PostTask(
      FROM_HERE, base::Bind(&WMFMediaPipeline::ThreadedImpl::Seek,
                            base::Unretained(threaded_impl_.get()), time));
}

void WMFMediaPipeline::SeekDone(bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(seek_cb_);
  base::ResetAndReturn(&seek_cb_).Run(success);
}

void WMFMediaPipeline::AudioConfigChanged(PlatformAudioConfig audio_config) {
  read_audio_data_cb_.Reset();
  audio_config_changed_cb_.Run(audio_config);
}

void WMFMediaPipeline::VideoConfigChanged(PlatformVideoConfig video_config) {
  read_video_data_cb_.Reset();
  video_config_changed_cb_.Run(video_config);
}

WMFMediaPipeline::ThreadedImpl::ThreadedImpl(DataSource* data_source)
    :  // We are constructing this object on the main thread
      main_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      data_source_(data_source),
      audio_timestamp_calculator_(new AudioTimestampCalculator),
      get_stride_function_(nullptr),
      weak_ptr_factory_(this) {
  std::fill(
      stream_indices_,
      stream_indices_ + PlatformMediaDataType::PLATFORM_MEDIA_DATA_TYPE_COUNT,
      static_cast<DWORD>(MF_SOURCE_READER_INVALID_STREAM_INDEX));
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

WMFMediaPipeline::ThreadedImpl::~ThreadedImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void WMFMediaPipeline::ThreadedImpl::Initialize(
    base::WeakPtr<WMFMediaPipeline> wmf_media_pipeline,
    scoped_refptr<WMFByteStream> byte_stream,
    const std::string& mime_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(data_source_);
  DCHECK(!source_reader_worker_.hasReader());

  wmf_media_pipeline_ = wmf_media_pipeline;

  PlatformMediaTimeInfo time_info;
  int bitrate = 0;
  PlatformAudioConfig audio_config;
  PlatformVideoConfig video_config;

  // We've already made this check in WebMediaPlayerImpl, but that's been in
  // a different process, so let's take its result with a grain of salt.
  const bool has_platform_support = IsPlatformMediaPipelineAvailable(
      PlatformMediaCheckType::FULL);

  get_stride_function_ = reinterpret_cast<decltype(get_stride_function_)>(
      GetFunctionFromLibrary("MFGetStrideForBitmapInfoHeader",
                                    "evr.dll"));

  if (!has_platform_support || !get_stride_function_) {
    LOG(WARNING) << " PROPMEDIA(GPU) : " << __FUNCTION__
                 << " Can't access required media libraries in the system";
    main_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&WMFMediaPipeline::InitializeDone, wmf_media_pipeline_,
                       false, bitrate, time_info, audio_config, video_config));
    return;
  }

  InitializeMediaFoundation();

  Microsoft::WRL::ComPtr<IMFAttributes> source_reader_attributes;
  if (!CreateSourceReaderCallbackAndAttributes(&source_reader_attributes)) {
    LOG(WARNING) << " PROPMEDIA(GPU) : " << __FUNCTION__
                 << " Failed to create source reader attributes";
    main_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&WMFMediaPipeline::InitializeDone, wmf_media_pipeline_,
                       false, bitrate, time_info, audio_config, video_config));
    return;
  }

  Microsoft::WRL::ComPtr<IMFSourceReader> source_reader;
  const HRESULT hr = MFCreateSourceReaderFromByteStream(
      byte_stream.get(), source_reader_attributes.Get(),
      source_reader.GetAddressOf());
  if (FAILED(hr)) {
    LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__
               << " Failed to create SOFTWARE source reader.";
    source_reader.Reset();
    main_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&WMFMediaPipeline::InitializeDone, wmf_media_pipeline_,
                       false, bitrate, time_info, audio_config, video_config));
    return;
  }

  source_reader_worker_.SetReader(source_reader);

  if (!RetrieveStreamIndices()) {
    LOG(WARNING) << " PROPMEDIA(GPU) : " << __FUNCTION__
                 << " Failed to find streams";
    main_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&WMFMediaPipeline::InitializeDone, wmf_media_pipeline_,
                       false, bitrate, time_info, audio_config, video_config));
    return;
  }

  if (!ConfigureSourceReader()) {
    LOG(WARNING) << " PROPMEDIA(GPU) : " << __FUNCTION__
                 << " Failed configure source reader";
    main_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&WMFMediaPipeline::InitializeDone, wmf_media_pipeline_,
                       false, bitrate, time_info, audio_config, video_config));
    return;
  }

  time_info.duration = GetDuration();
  bitrate = GetBitrate(time_info.duration);

  if (HasMediaStream(PlatformMediaDataType::PLATFORM_MEDIA_AUDIO)) {
    if (!GetAudioDecoderConfig(&audio_config)) {
      LOG(WARNING) << " PROPMEDIA(GPU) : " << __FUNCTION__
                   << " Failed to get Audio Decoder Config";
      main_task_runner_->PostTask(
          FROM_HERE, base::BindOnce(&WMFMediaPipeline::InitializeDone,
                                    wmf_media_pipeline_, false, bitrate,
                                    time_info, audio_config, video_config));
      return;
    }
  }

  if (HasMediaStream(PlatformMediaDataType::PLATFORM_MEDIA_VIDEO)) {
    if (!GetVideoDecoderConfig(&video_config)) {
      LOG(WARNING) << " PROPMEDIA(GPU) : " << __FUNCTION__
                   << " Failed to get Video Decoder Config";
      main_task_runner_->PostTask(
          FROM_HERE, base::BindOnce(&WMFMediaPipeline::InitializeDone,
                                    wmf_media_pipeline_, false, bitrate,
                                    time_info, audio_config, video_config));
      return;
    }
  }

  VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
          << " FinalizeInitialization successful with PlatformAudioConfig :"
          << Loggable(audio_config);
  VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
          << " FinalizeInitialization successful with PlatformVideoConfig :"
          << Loggable(video_config);

  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WMFMediaPipeline::InitializeDone, wmf_media_pipeline_,
                     true, bitrate, time_info, audio_config, video_config));
  return;
}

void WMFMediaPipeline::ThreadedImpl::ReadData(PlatformMediaDataType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // We might have some data ready to send.
  if (pending_decoded_data_[type]) {
    scoped_refptr<DataBuffer> decoded_data = pending_decoded_data_[type];
    pending_decoded_data_[type] = nullptr;
    main_task_runner_->PostTask(
        FROM_HERE, base::Bind(&WMFMediaPipeline::ReadDataDone,
                              wmf_media_pipeline_, type, decoded_data));
    return;
  }

  DCHECK(source_reader_worker_.hasReader());

  HRESULT hr = source_reader_worker_.ReadSampleAsync(stream_indices_[type]);

  if (FAILED(hr)) {
    LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__
               << " Failed to read audio sample";
    main_task_runner_->PostTask(FROM_HERE,
                                base::Bind(&WMFMediaPipeline::ReadDataDone,
                                           wmf_media_pipeline_, type, nullptr));
  }
}

void WMFMediaPipeline::ThreadedImpl::OnReadSample(
    MediaDataStatus status,
    DWORD stream_index,
    const Microsoft::WRL::ComPtr<IMFSample>& sample) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  PlatformMediaDataType media_type =
      PlatformMediaDataType::PLATFORM_MEDIA_AUDIO;
  if (stream_index ==
      stream_indices_[PlatformMediaDataType::PLATFORM_MEDIA_VIDEO]) {
    media_type = PlatformMediaDataType::PLATFORM_MEDIA_VIDEO;
  } else if (stream_index !=
             stream_indices_[PlatformMediaDataType::PLATFORM_MEDIA_AUDIO]) {
    NOTREACHED() << "Unknown stream type";
  }
  DCHECK(!pending_decoded_data_[media_type]);

  scoped_refptr<DataBuffer> data_buffer;
  switch (status) {
    case MediaDataStatus::kOk:
      DCHECK(sample);
      data_buffer = CreateDataBuffer(sample.Get(), media_type);
      break;

    case MediaDataStatus::kEOS:
      data_buffer = DataBuffer::CreateEOSBuffer();
      break;

    case MediaDataStatus::kMediaError:
      LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__
                 << " status MediaDataStatus::kMediaError";
      break;

    case MediaDataStatus::kConfigChanged: {
      // Chromium's pipeline does not want any decoded data when we report
      // that configuration has changed. We need to buffer the sample and
      // send it during next read operation.
      pending_decoded_data_[media_type] =
          CreateDataBuffer(sample.Get(), media_type);

      if (media_type == PlatformMediaDataType::PLATFORM_MEDIA_AUDIO) {
        PlatformAudioConfig audio_config;
        if (GetAudioDecoderConfig(&audio_config)) {
          VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
                  << Loggable(audio_config);
          main_task_runner_->PostTask(
              FROM_HERE, base::Bind(&WMFMediaPipeline::AudioConfigChanged,
                                    wmf_media_pipeline_, audio_config));
          return;
        }

        LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__
                   << " Error while getting decoder audio configuration"
                   << " changing status to MediaDataStatus::kMediaError";
        status = MediaDataStatus::kMediaError;
        break;
      } else if (media_type == PlatformMediaDataType::PLATFORM_MEDIA_VIDEO) {
        PlatformVideoConfig video_config;
        if (GetVideoDecoderConfig(&video_config)) {
          main_task_runner_->PostTask(
              FROM_HERE, base::Bind(&WMFMediaPipeline::VideoConfigChanged,
                                    wmf_media_pipeline_, video_config));
          return;
        }

        LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__
                   << " Error while getting decoder video configuration"
                   << " changing status to MediaDataStatus::kMediaError";
        status = MediaDataStatus::kMediaError;
        break;
      }
      // Fallthrough.
      FALLTHROUGH;
    }

    default:
      NOTREACHED();
  }

  main_task_runner_->PostTask(
      FROM_HERE, base::Bind(&WMFMediaPipeline::ReadDataDone,
                            wmf_media_pipeline_, media_type, data_buffer));
}

scoped_refptr<DataBuffer>
WMFMediaPipeline::ThreadedImpl::CreateDataBufferFromMemory(IMFSample* sample) {
  // Get a pointer to the IMFMediaBuffer in the sample.
  Microsoft::WRL::ComPtr<IMFMediaBuffer> output_buffer;
  HRESULT hr = sample->ConvertToContiguousBuffer(output_buffer.GetAddressOf());
  if (FAILED(hr)) {
    LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__
               << " Failed to get pointer to data in sample.";
    return nullptr;
  }

  // Get the actual data from the IMFMediaBuffer
  uint8_t* data = nullptr;
  DWORD data_size = 0;
  hr = output_buffer->Lock(&data, NULL, &data_size);
  if (FAILED(hr)) {
    LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__
               << " Failed to lock buffer.";
    return nullptr;
  }
  scoped_refptr<DataBuffer> data_buffer =
      DataBuffer::CopyFrom(data, data_size);

  // Unlock the IMFMediaBuffer buffer.
  output_buffer->Unlock();

  return data_buffer;
}

scoped_refptr<DataBuffer> WMFMediaPipeline::ThreadedImpl::CreateDataBuffer(
    IMFSample* sample,
    PlatformMediaDataType media_type) {
  scoped_refptr<DataBuffer> data_buffer;

  data_buffer = CreateDataBufferFromMemory(sample);
  if (!data_buffer)
    return nullptr;

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

  if (media_type == PlatformMediaDataType::PLATFORM_MEDIA_AUDIO) {
    // We calculate the timestamp and the duration based on the number of
    // audio frames we've already played. We don't trust the timestamp
    // stored on the IMFSample, as sometimes it's wrong, possibly due to
    // buggy encoders?
    data_buffer->set_timestamp(audio_timestamp_calculator_->GetTimestamp(
        timestamp_hns, discontinuity != 0));
    int64_t frames_count = audio_timestamp_calculator_->GetFramesCount(
        static_cast<int64_t>(data_buffer->data_size()));
    data_buffer->set_duration(
        audio_timestamp_calculator_->GetDuration(frames_count));
    audio_timestamp_calculator_->UpdateFrameCounter(frames_count);
  } else if (media_type == PlatformMediaDataType::PLATFORM_MEDIA_VIDEO) {
    data_buffer->set_timestamp(
        base::TimeDelta::FromMicroseconds(timestamp_hns / 10));
    data_buffer->set_duration(
        base::TimeDelta::FromMicroseconds(duration_hns / 10));
  }

  return data_buffer;
}

void WMFMediaPipeline::ThreadedImpl::Seek(base::TimeDelta time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Seek requests on a streaming data source can confuse WMF.
  // Chromium sometimes seeks to the beginning of a stream when starting up.
  // Since that should be a no-op, we just pretend it succeeded.
  if (data_source_->IsStreaming() && time == base::TimeDelta()) {
    main_task_runner_->PostTask(
        FROM_HERE, base::Bind(&WMFMediaPipeline::SeekDone, wmf_media_pipeline_,
                              true));
    return;
  }

  AutoPropVariant position;
  // IMFSourceReader::SetCurrentPosition expects position in 100-nanosecond
  // units, so we have to multiply time in microseconds by 10.
  HRESULT hr =
      InitPropVariantFromInt64(time.InMicroseconds() * 10, position.get());
  if (FAILED(hr)) {
    main_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&WMFMediaPipeline::SeekDone, wmf_media_pipeline_, false));
    return;
  }

  audio_timestamp_calculator_->RecapturePosition();
  hr = source_reader_worker_.SetCurrentPosition(position);
  main_task_runner_->PostTask(
      FROM_HERE, base::Bind(&WMFMediaPipeline::SeekDone, wmf_media_pipeline_,
                            SUCCEEDED(hr)));
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
  DCHECK(source_reader_worker_.hasReader());

  // In case of some audio streams SourceReader might not get everything
  // right just from examining the stream (i.e. during initialization), so some
  // of the values reported here might be wrong. In such case first sample
  // shall be decoded with |MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED| status,
  // what will allow us to get proper configuration.

  audio_config->format = SampleFormat::kSampleFormatF32;

  Microsoft::WRL::ComPtr<IMFMediaType> media_type;
  HRESULT hr = source_reader_worker_.GetCurrentMediaType(
      stream_indices_[PlatformMediaDataType::PLATFORM_MEDIA_AUDIO], media_type);

  if (FAILED(hr) || !media_type) {
    LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__
               << " Failed to obtain audio media type.";
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
  DCHECK(source_reader_worker_.hasReader());

  // In case of some video streams SourceReader might not get everything
  // right just from examining the stream (i.e. during initialization), so some
  // of the values reported here might be wrong. In such case first sample
  // shall be decoded with |MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED| status,
  // what will allow us to get proper configuration.

  Microsoft::WRL::ComPtr<IMFMediaType> media_type;
  HRESULT hr = source_reader_worker_.GetCurrentMediaType(
      stream_indices_[PlatformMediaDataType::PLATFORM_MEDIA_VIDEO], media_type);

  if (FAILED(hr)) {
    LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__
               << " Failed to obtain video media type.";
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
    Microsoft::WRL::ComPtr<IMFAttributes>* attributes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!source_reader_callback_.Get());

  source_reader_callback_ = new SourceReaderCallback(BindToCurrentLoop(
      base::Bind(&WMFMediaPipeline::ThreadedImpl::OnReadSample,
                 weak_ptr_factory_.GetWeakPtr())));

  HRESULT hr = MFCreateAttributes((*attributes).GetAddressOf(), 1);
  if (FAILED(hr)) {
    source_reader_callback_.Reset();
    return false;
  }

  hr = (*attributes)
           ->SetUnknown(MF_SOURCE_READER_ASYNC_CALLBACK,
                        source_reader_callback_.Get());
  if (FAILED(hr))
    return false;

  return true;
}

bool WMFMediaPipeline::ThreadedImpl::RetrieveStreamIndices() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(source_reader_worker_.hasReader());

  DWORD stream_index = 0;
  HRESULT hr = S_OK;

  while (!(HasMediaStream(PlatformMediaDataType::PLATFORM_MEDIA_AUDIO) &&
           HasMediaStream(PlatformMediaDataType::PLATFORM_MEDIA_VIDEO))) {
    Microsoft::WRL::ComPtr<IMFMediaType> media_type;
    hr = source_reader_worker_.GetNativeMediaType(stream_index, media_type);

    if (hr == MF_E_INVALIDSTREAMNUMBER)
      break;  // No more streams.

    if (SUCCEEDED(hr)) {
      GUID major_type;
      hr = media_type->GetMajorType(&major_type);
      if (SUCCEEDED(hr)) {
        if (major_type == MFMediaType_Audio &&
            stream_indices_[PlatformMediaDataType::PLATFORM_MEDIA_AUDIO] ==
                static_cast<DWORD>(MF_SOURCE_READER_INVALID_STREAM_INDEX)) {
          stream_indices_[PlatformMediaDataType::PLATFORM_MEDIA_AUDIO] = stream_index;
        } else if (major_type == MFMediaType_Video &&
                   stream_indices_[PlatformMediaDataType::PLATFORM_MEDIA_VIDEO] ==
                       static_cast<DWORD>(MF_SOURCE_READER_INVALID_STREAM_INDEX)) {
          stream_indices_[PlatformMediaDataType::PLATFORM_MEDIA_VIDEO] = stream_index;
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
  DCHECK(source_reader_worker_.hasReader());

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
               << " Failed to set media subtype.";
    return false;
  }

  hr = source_reader_worker_.SetCurrentMediaType(
      stream_index, new_current_media_type);
  if (FAILED(hr)) {
    LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__
               << " Failed to set media type. No "
               << (is_video ? "video" : "audio") << " track?";
    return false;
  }

  // When we set the media type without providing complete media information
  // WMF tries to figure it out on its own.  But it doesn't do it until it's
  // needed -- e.g., when decoding is requested.  Since this figuring-out
  // process can fail, let's force it now by calling GetCurrentMediaType().
  Microsoft::WRL::ComPtr<IMFMediaType> media_type;
  hr = source_reader_worker_.GetCurrentMediaType(stream_index, media_type);

  if (FAILED(hr)) {
    LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__
               << " Failed to obtain media type.";
    return false;
  }

  return true;
}

bool WMFMediaPipeline::ThreadedImpl::ConfigureSourceReader() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(source_reader_worker_.hasReader());

  static const PlatformMediaDataType media_types[] = {
      PlatformMediaDataType::PLATFORM_MEDIA_AUDIO, PlatformMediaDataType::PLATFORM_MEDIA_VIDEO};
  static_assert(arraysize(media_types) == PlatformMediaDataType::PLATFORM_MEDIA_DATA_TYPE_COUNT,
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
  DCHECK(source_reader_worker_.hasReader());

  AutoPropVariant var;

  HRESULT hr = source_reader_worker_.GetDuration(var);
  if (FAILED(hr)) {
    DLOG_IF(WARNING, !data_source_->IsStreaming())
        << "Failed to obtain media duration.";
    return media::kInfiniteDuration;
  }

  int64_t duration_int64 = 0;
  hr = var.ToInt64(&duration_int64);
  if (FAILED(hr)) {
    LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__
               << " Failed to obtain media duration.";
    return media::kInfiniteDuration;
  }
  // Have to divide duration64 by ten to convert from
  // hundreds of nanoseconds (WMF style) to microseconds.
  return base::TimeDelta::FromMicroseconds(duration_int64 / 10);
}

int WMFMediaPipeline::ThreadedImpl::GetBitrate(base::TimeDelta duration) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(source_reader_worker_.hasReader());
  DCHECK_GT(duration.InMicroseconds(), 0);

  AutoPropVariant var;

  // Calculating the media bitrate
  HRESULT hr = source_reader_worker_.GetAudioBitrate(var);
  int audio_bitrate = 0;
  hr = var.ToInt32(&audio_bitrate);
  if (FAILED(hr))
    audio_bitrate = 0;

  hr = source_reader_worker_.GetVideoBitrate(var);
  int video_bitrate = 0;
  hr = var.ToInt32(&video_bitrate);
  if (FAILED(hr))
    video_bitrate = 0;

  const int bitrate = std::max(audio_bitrate + video_bitrate, 0);
  if (bitrate == 0 && !data_source_->IsStreaming()) {
    // If we have a valid bitrate we can use it, otherwise we have to calculate
    // it from file size and duration.
      hr = source_reader_worker_.GetFileSize(var);
    if (SUCCEEDED(hr) && duration.InMicroseconds() > 0) {
      int64_t file_size_in_bytes;
      hr = var.ToInt64(&file_size_in_bytes);
      if (SUCCEEDED(hr))
        return (8000000.0 * file_size_in_bytes) / duration.InMicroseconds();
    }
    LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__
               << " Failed to obtain media bitrate.";
  }

  return bitrate;
}

bool WMFMediaPipeline::ThreadedImpl::GetStride(int* stride) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(source_reader_worker_.hasReader());

  Microsoft::WRL::ComPtr<IMFMediaType> media_type;
  HRESULT hr = source_reader_worker_.GetCurrentMediaType(
      stream_indices_[PlatformMediaDataType::PLATFORM_MEDIA_VIDEO], media_type);

  if (FAILED(hr)) {
    LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__
               << " Failed to obtain media type.";
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
}  // namespace media
