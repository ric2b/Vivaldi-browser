// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#include "platform_media/gpu/pipeline/win/source_reader_worker.h"

#include <propvarutil.h>

using namespace platform_media;

AutoPropVariant::AutoPropVariant() {
  PropVariantInit(&var);
}

AutoPropVariant::~AutoPropVariant() {
  PropVariantClear(&var);
}

HRESULT AutoPropVariant::ToInt64(LONGLONG* ret) {
  return PropVariantToInt64(var, ret);
}

HRESULT AutoPropVariant::ToInt32(int* ret) {
  return PropVariantToInt32(var, ret);
}

SourceReaderWorker::SourceReaderWorker() {

}

SourceReaderWorker::~SourceReaderWorker() {

}

void SourceReaderWorker::SetReader(
    const Microsoft::WRL::ComPtr<IMFSourceReader>& source_reader) {
  source_reader_ = source_reader;
}

// Read the next sample using asynchronous mode.
// http://msdn.microsoft.com/en-us/library/windows/desktop/gg583871(v=vs.85).aspx
HRESULT SourceReaderWorker::ReadSampleAsync(DWORD index) {
  VLOG(7) << " PROPMEDIA(GPU) : " << __FUNCTION__
          << ": index " << index;

  HRESULT hr =
    source_reader_->ReadSample(index, 0, nullptr, nullptr, nullptr, nullptr);

  LOG_IF(ERROR, FAILED(hr))
      << " PROPMEDIA(GPU) : " << __FUNCTION__
      << " : Received an error";

  return hr;
}

HRESULT SourceReaderWorker::SetCurrentPosition(AutoPropVariant & position) {
  VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__;

  HRESULT hr = source_reader_->SetCurrentPosition(GUID_NULL, position.get_ref());

  LOG_IF(ERROR, FAILED(hr))
      << " PROPMEDIA(GPU) : " << __FUNCTION__
      << " : Received an error";

  return hr;
}

HRESULT SourceReaderWorker::GetCurrentMediaType(
    DWORD index,
    Microsoft::WRL::ComPtr<IMFMediaType> & media_type) {
  VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
          << ": index " << index;

  HRESULT hr = source_reader_->GetCurrentMediaType(index, media_type.GetAddressOf());

  LOG_IF(ERROR, FAILED(hr))
      << " PROPMEDIA(GPU) : " << __FUNCTION__
      << " : Received an error";

  return hr;
}

HRESULT SourceReaderWorker::SetCurrentMediaType(
    DWORD index,
    Microsoft::WRL::ComPtr<IMFMediaType> & media_type) {
  VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
          << ": index " << index;

  HRESULT hr = source_reader_->SetCurrentMediaType(
    index, NULL, media_type.Get());

  LOG_IF(ERROR, FAILED(hr))
      << " PROPMEDIA(GPU) : " << __FUNCTION__
      << " : Received an error";

  return hr;
}

HRESULT SourceReaderWorker::GetNativeMediaType(
    DWORD index,
    Microsoft::WRL::ComPtr<IMFMediaType> & media_type) {
  VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
          << ": index " << index;

  HRESULT hr = source_reader_->GetNativeMediaType(index, 0, media_type.GetAddressOf());

  LOG_IF(ERROR, FAILED(hr))
      << " PROPMEDIA(GPU) : " << __FUNCTION__
      << " : Received an error";

  return hr;
}

HRESULT SourceReaderWorker::GetDuration(AutoPropVariant & var) {
  VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__;

  HRESULT hr = source_reader_->GetPresentationAttribute(
      static_cast<DWORD>(MF_SOURCE_READER_MEDIASOURCE),
      MF_PD_DURATION,
      var.get());

  LOG_IF(ERROR, FAILED(hr))
      << " PROPMEDIA(GPU) : " << __FUNCTION__
      << " : Received an error";

  return hr;
}

HRESULT SourceReaderWorker::GetAudioBitrate(AutoPropVariant & var) {
  VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__;

  HRESULT hr = source_reader_->GetPresentationAttribute(
      static_cast<DWORD>(MF_SOURCE_READER_MEDIASOURCE),
      MF_PD_AUDIO_ENCODING_BITRATE,
      var.get());

  LOG_IF(ERROR, FAILED(hr))
      << " PROPMEDIA(GPU) : " << __FUNCTION__
      << " : Received an error";

  return hr;
}

HRESULT SourceReaderWorker::GetVideoBitrate(AutoPropVariant & var) {
  VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__;

  HRESULT hr = source_reader_->GetPresentationAttribute(
      static_cast<DWORD>(MF_SOURCE_READER_MEDIASOURCE),
      MF_PD_VIDEO_ENCODING_BITRATE,
      var.get());

  LOG_IF(ERROR, FAILED(hr))
      << " PROPMEDIA(GPU) : " << __FUNCTION__
      << " : Received an error";

  return hr;
}

HRESULT SourceReaderWorker::GetFileSize(AutoPropVariant & var) {
  VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__;

  HRESULT hr = source_reader_->GetPresentationAttribute(
      static_cast<DWORD>(MF_SOURCE_READER_MEDIASOURCE),
      MF_PD_TOTAL_FILE_SIZE,
      var.get());

  LOG_IF(ERROR, FAILED(hr))
      << " PROPMEDIA(GPU) : " << __FUNCTION__
      << " : Received an error";

  return hr;
}
