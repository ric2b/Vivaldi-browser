// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2013 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#include "platform_media/gpu/decoders/win/wmf_byte_stream.h"

#include "platform_media/gpu/decoders/win/read_stream.h"

#include <algorithm>

namespace media {

namespace {

class WMFReadRequest : public base::win::IUnknownImpl {
 public:
  WMFReadRequest(BYTE* buff, ULONG len) : buffer(buff), length(len), read(0) {
  }

  ~WMFReadRequest() override {}

  BYTE* buffer;
  ULONG length;
  ULONG read;
};

}  // namespace

WMFByteStream::WMFByteStream(DataSource* data_source)
    : async_result_(nullptr),
      read_stream_(new ReadStream(data_source)) {
  VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__;
}

WMFByteStream::~WMFByteStream() {
  VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__;
  DCHECK(read_stream_->HasStopped());
}

HRESULT WMFByteStream::Initialize(LPCWSTR mime_type) {
  DCHECK(thread_checker_.CalledOnValidThread());

  HRESULT hresult = MFCreateAttributes(attributes_.GetAddressOf(), 1);
  if (SUCCEEDED(hresult)) {
    attributes_->SetString(MF_BYTESTREAM_CONTENT_TYPE, mime_type);
    read_stream_->Initialize(this);
    return S_OK;
  }

  LOG(WARNING) << " PROPMEDIA(GPU) : " << __FUNCTION__
               << " (E_ABORT) MFCreateAttributes failed";
  return E_ABORT;
}

void WMFByteStream::Stop() {
  VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());
  read_stream_->Stop();

  if (async_result_) {
    // Set async_result_ to NULL before calling MFInvokeCallback. Doing
    // it after that call creates race condition because BeginRead may
    // be called before this function returns. Callback will pass result
    // to EndProc where it will be released.
    IMFAsyncResult* result = async_result_;
    async_result_ = NULL;
    LOG(WARNING) << " PROPMEDIA(GPU) : " << __FUNCTION__
                 << " (E_INVALIDARG) Stopped while in async read";
    result->SetStatus(E_INVALIDARG);
    MFInvokeCallback(result);
  }
}

HRESULT STDMETHODCALLTYPE
WMFByteStream::QueryInterface(REFIID riid, void** object) {
  if (riid == IID_IMFByteStream) {
    *object = static_cast<IMFByteStream*>(this);
    AddRef();
    return S_OK;
  } else if (riid == IID_IMFAttributes) {
    *object = static_cast<IMFAttributes*>(this);
    AddRef();
    return S_OK;
  }

  return IUnknownImpl::QueryInterface(riid, object);
}

ULONG STDMETHODCALLTYPE WMFByteStream::AddRef() {
  return IUnknownImpl::AddRef();
}

ULONG STDMETHODCALLTYPE WMFByteStream::Release() {
  return IUnknownImpl::Release();
}

HRESULT STDMETHODCALLTYPE
WMFByteStream::GetCapabilities(DWORD* capabilities) {
  *capabilities =
      MFBYTESTREAM_IS_READABLE | MFBYTESTREAM_IS_SEEKABLE |
      (read_stream_->IsStreaming()
           ? (MFBYTESTREAM_HAS_SLOW_SEEK | MFBYTESTREAM_IS_PARTIALLY_DOWNLOADED)
           : 0);

  return S_OK;
}

HRESULT STDMETHODCALLTYPE WMFByteStream::GetLength(QWORD* length) {
  *length = read_stream_->Size();
  return S_OK;
}

HRESULT STDMETHODCALLTYPE WMFByteStream::SetLength(QWORD length) {
  // The stream is not writable, so do nothing here.
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE
WMFByteStream::GetCurrentPosition(QWORD* position) {
  *position = read_stream_->CurrentPosition();
  return S_OK;
}

HRESULT STDMETHODCALLTYPE WMFByteStream::SetCurrentPosition(QWORD position) {
  if (static_cast<int64_t>(position) < 0) {
    LOG(WARNING) << " PROPMEDIA(GPU) : " << __FUNCTION__
                 << " (E_INVALIDARG) Invalid position";
    return E_INVALIDARG;  // might happen if the stream is not seekable or
                          // if the position overflows the stream
  }

  if(read_stream_->IsStreaming()) {
    VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
            << " Cannot SetCurrentPosition to " << position
            << " Media is streaming";
  }
  else
  {
    VLOG(7) << " PROPMEDIA(GPU) : " << __FUNCTION__
            << " SetCurrentPosition " << position;
    read_stream_->SetCurrentPosition(position);
  }
  return S_OK;
}

HRESULT STDMETHODCALLTYPE WMFByteStream::IsEndOfStream(BOOL* end_of_stream) {
  bool eof = read_stream_->IsEndOfStream();
  *end_of_stream = eof;
  return S_OK;
}

HRESULT STDMETHODCALLTYPE
WMFByteStream::Read(BYTE* buff, ULONG len, ULONG* read) {
  DCHECK(!thread_checker_.CalledOnValidThread())
      << "Trying to make a blocking read on the main thread";

  if (read_stream_->HasStopped()) {
    // NOTE(pettern@vivaldi.com): Do not try to read when we've stopped
    // as it might cause freezes with the read_done event never firing.
    LOG(WARNING) << " PROPMEDIA(GPU) : " << __FUNCTION__
                 << " (E_FAIL) Stream has stopped";
    return E_FAIL;
  }

  int bytes_read = read_stream_->SyncRead(buff, len);

  if (bytes_read == DataSource::kReadError) {
    LOG(WARNING) << " PROPMEDIA(GPU) : " << __FUNCTION__
                 << " (E_FAIL) Stream sync read error";
    return E_FAIL;
  }

  *read = bytes_read;
  return S_OK;
}

void WMFByteStream::OnReadData(int size) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (read_stream_->HasStopped()) {
    LOG(WARNING) << " PROPMEDIA(GPU) : " << __FUNCTION__
                 << " Stream has stopped";
    return;
  }

  Microsoft::WRL::ComPtr<IUnknown> unknown;
  HRESULT hr = async_result_->GetObjectW(unknown.GetAddressOf());
  WMFReadRequest* read_request = static_cast<WMFReadRequest*>(unknown.Get());

  HRESULT status;
  if (FAILED(hr) || !unknown || size == DataSource::kReadError) {
    LOG(WARNING) << " PROPMEDIA(GPU) : " << __FUNCTION__
                 << " (E_FAIL Stream async read error";
    status = E_FAIL;
  } else {
    DCHECK(static_cast<ULONG>(size) <= read_request->length);
    read_request->read = size;
    status = S_OK;
  }

  // Setting async_result_ to NULL here to avoid second invoke of the callback
  // from Stop function. When doing so we also have to use local variable and
  // reset async_result_ before calling MFInvokeCallback, otherwise there would
  // be a race condition, because BeginRead may be called before this function
  // returns. Callback will pass result to EndRead where it will be released.
  IMFAsyncResult* result = async_result_;
  async_result_ = NULL;
  result->SetStatus(status);
  MFInvokeCallback(result);
}

HRESULT STDMETHODCALLTYPE WMFByteStream::BeginRead(BYTE* buff,
                                                   ULONG len,
                                                   IMFAsyncCallback* callback,
                                                   IUnknown* state) {
  if (read_stream_->HasStopped()) {
    LOG(WARNING) << " PROPMEDIA(GPU) : " << __FUNCTION__
                 << " (E_INVALIDARG) Stream has stopped";
    return E_INVALIDARG;
  }

  if (async_result_) {
    LOG(WARNING) << " PROPMEDIA(GPU) : " << __FUNCTION__
                 << " (E_ABORT) Stream in Progress";
    return E_ABORT;
  }

  Microsoft::WRL::ComPtr<IUnknown> read_request(new WMFReadRequest(buff, len));
  HRESULT hresult =
      MFCreateAsyncResult(read_request.Get(), callback, state, &async_result_);
  if (FAILED(hresult)) {
    LOG(WARNING) << " PROPMEDIA(GPU) : " << __FUNCTION__
                 << " (E_ABORT) MFCreateAsyncResult failed";
    return E_ABORT;
  }

  read_stream_->AsyncRead(buff, len);
  return S_OK;
}

HRESULT STDMETHODCALLTYPE
WMFByteStream::EndRead(IMFAsyncResult* result, ULONG* read) {
  HRESULT hresult;
  Microsoft::WRL::ComPtr<IUnknown> unknown;
  if (FAILED(result->GetObjectW(unknown.GetAddressOf())) || !unknown) {
    LOG(WARNING) << " PROPMEDIA(GPU) : " << __FUNCTION__
                 << " (E_INVALIDARG) Stream has failed";
    hresult = E_INVALIDARG;
  } else {
    WMFReadRequest* read_request = static_cast<WMFReadRequest*>(unknown.Get());
    *read = read_request->read;
    hresult = result->GetStatus();
  }

  // was acquired by a call to MFCreateAsyncResult in BeginRead
  result->Release();
  return hresult;
}

HRESULT STDMETHODCALLTYPE
WMFByteStream::Write(const BYTE* buff, ULONG len, ULONG* written) {
  // The stream is not writable, so do nothing here.
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WMFByteStream::BeginWrite(const BYTE* buff,
                                                    ULONG len,
                                                    IMFAsyncCallback* callback,
                                                    IUnknown* punk_state) {
  // The stream is not writable, so do nothing here.
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE
WMFByteStream::EndWrite(IMFAsyncResult* result, ULONG* written) {
  // The stream is not writable, so do nothing here.
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE
WMFByteStream::Seek(MFBYTESTREAM_SEEK_ORIGIN seek_origin,
                    LONGLONG seek_offset,
                    DWORD seek_flags,
                    QWORD* current_position) {
  int64_t size = read_stream_->Size();
  switch (seek_origin) {
    case msoBegin:
      if ((size > 0 && seek_offset > size) || seek_offset < 0) {
        LOG(WARNING) << " PROPMEDIA(GPU) : " << __FUNCTION__
                     << " (E_INVALIDARG) Invalid Seek";
        return E_INVALIDARG;  // might happen if the stream is not seekable or
                              // if the llSeekOffset overflows the stream
      }

      VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
              << " SetCurrentPosition " << seek_offset;
      read_stream_->SetCurrentPosition(seek_offset);
      break;

    case msoCurrent:
      int64_t current_position = read_stream_->CurrentPosition();
      if ((size > 0 && (current_position + seek_offset) > size) ||
          (current_position + seek_offset) < 0) {
        LOG(WARNING) << " PROPMEDIA(GPU) : " << __FUNCTION__
                     << " (E_INVALIDARG) Invalid Seek";
        return E_INVALIDARG;  // might happen if the stream is not seekable or
                              // if the llSeekOffset overflows the stream
      }

      VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
              << " SetCurrentPosition " << (current_position + seek_offset);
      read_stream_->SetCurrentPosition(current_position + seek_offset);
      break;
  }

  *current_position = read_stream_->CurrentPosition();
  return S_OK;
}

HRESULT STDMETHODCALLTYPE WMFByteStream::Flush() {
  // The stream is not writable, so do nothing here.
  return S_OK;
}

HRESULT STDMETHODCALLTYPE WMFByteStream::Close() {
  return S_OK;
}

HRESULT STDMETHODCALLTYPE
WMFByteStream::GetItem(REFGUID guid_key, PROPVARIANT* value) {
  if (attributes_.Get()) {
    return attributes_->GetItem(guid_key, value);
  } else {
    return E_FAIL;
  }
}

HRESULT STDMETHODCALLTYPE
WMFByteStream::GetItemType(REFGUID guid_key, MF_ATTRIBUTE_TYPE* type) {
  if (attributes_.Get()) {
    return attributes_->GetItemType(guid_key, type);
  } else {
    return E_FAIL;
  }
}

HRESULT STDMETHODCALLTYPE WMFByteStream::CompareItem(REFGUID guid_key,
                                                     REFPROPVARIANT value,
                                                     BOOL* result) {
  if (attributes_.Get()) {
    return attributes_->CompareItem(guid_key, value, result);
  } else {
    return E_FAIL;
  }
}

HRESULT STDMETHODCALLTYPE
WMFByteStream::Compare(IMFAttributes* theirs,
                       MF_ATTRIBUTES_MATCH_TYPE match_type,
                       BOOL* result) {
  if (attributes_.Get()) {
    return attributes_->Compare(theirs, match_type, result);
  } else {
    return E_FAIL;
  }
}

HRESULT STDMETHODCALLTYPE
WMFByteStream::GetUINT32(REFGUID guid_key, UINT32* value) {
  if (attributes_.Get()) {
    return attributes_->GetUINT32(guid_key, value);
  } else {
    return E_FAIL;
  }
}

HRESULT STDMETHODCALLTYPE
WMFByteStream::GetUINT64(REFGUID guid_key, UINT64* value) {
  if (attributes_.Get()) {
    return attributes_->GetUINT64(guid_key, value);
  } else {
    return E_FAIL;
  }
}

HRESULT STDMETHODCALLTYPE
WMFByteStream::GetDouble(REFGUID guid_key, double* value) {
  if (attributes_.Get()) {
    return attributes_->GetDouble(guid_key, value);
  } else {
    return E_FAIL;
  }
}

HRESULT STDMETHODCALLTYPE
WMFByteStream::GetGUID(REFGUID guid_key, GUID* guid_value) {
  if (attributes_.Get()) {
    return attributes_->GetGUID(guid_key, guid_value);
  } else {
    return E_FAIL;
  }
}

HRESULT STDMETHODCALLTYPE
WMFByteStream::GetStringLength(REFGUID guid_key, UINT32* length) {
  if (attributes_.Get()) {
    return attributes_->GetStringLength(guid_key, length);
  } else {
    return E_FAIL;
  }
}

HRESULT STDMETHODCALLTYPE WMFByteStream::GetString(REFGUID guid_key,
                                                   LPWSTR value,
                                                   UINT32 buf_size,
                                                   UINT32* length) {
  if (attributes_.Get()) {
    return attributes_->GetString(guid_key, value, buf_size, length);
  } else {
    return E_FAIL;
  }
}

HRESULT STDMETHODCALLTYPE
WMFByteStream::GetAllocatedString(REFGUID guid_key,
                                  LPWSTR* value,
                                  UINT32* length) {
  if (attributes_.Get()) {
    return attributes_->GetAllocatedString(guid_key, value, length);
  } else {
    return E_FAIL;
  }
}

HRESULT STDMETHODCALLTYPE
WMFByteStream::GetBlobSize(REFGUID guid_key, UINT32* blob_size) {
  if (attributes_.Get()) {
    return attributes_->GetBlobSize(guid_key, blob_size);
  } else {
    return E_FAIL;
  }
}

HRESULT STDMETHODCALLTYPE WMFByteStream::GetBlob(REFGUID guid_key,
                                                 UINT8* buf,
                                                 UINT32 buf_size,
                                                 UINT32* blob_size) {
  if (attributes_.Get()) {
    return attributes_->GetBlob(guid_key, buf, buf_size, blob_size);
  } else {
    return E_FAIL;
  }
}

HRESULT STDMETHODCALLTYPE WMFByteStream::GetAllocatedBlob(REFGUID guid_key,
                                                          UINT8** buf,
                                                          UINT32* size) {
  if (attributes_.Get()) {
    return attributes_->GetAllocatedBlob(guid_key, buf, size);
  } else {
    return E_FAIL;
  }
}

HRESULT STDMETHODCALLTYPE
WMFByteStream::GetUnknown(REFGUID guid_key, REFIID riid, LPVOID* ppv) {
  if (attributes_.Get()) {
    return attributes_->GetUnknown(guid_key, riid, ppv);
  } else {
    return E_FAIL;
  }
}

HRESULT STDMETHODCALLTYPE
WMFByteStream::SetItem(REFGUID guid_key, REFPROPVARIANT value) {
  if (attributes_.Get()) {
    return attributes_->SetItem(guid_key, value);
  } else {
    return E_FAIL;
  }
}

HRESULT STDMETHODCALLTYPE WMFByteStream::DeleteItem(REFGUID guid_key) {
  if (attributes_.Get()) {
    return attributes_->DeleteItem(guid_key);
  } else {
    return E_FAIL;
  }
}

HRESULT STDMETHODCALLTYPE WMFByteStream::DeleteAllItems() {
  if (attributes_.Get()) {
    return attributes_->DeleteAllItems();
  } else {
    return E_FAIL;
  }
}

HRESULT STDMETHODCALLTYPE
WMFByteStream::SetUINT32(REFGUID guid_key, UINT32 value) {
  if (attributes_.Get()) {
    return attributes_->SetUINT32(guid_key, value);
  } else {
    return E_FAIL;
  }
}

HRESULT STDMETHODCALLTYPE
WMFByteStream::SetUINT64(REFGUID guid_key, UINT64 value) {
  if (attributes_.Get()) {
    return attributes_->SetUINT64(guid_key, value);
  } else {
    return E_FAIL;
  }
}

HRESULT STDMETHODCALLTYPE
WMFByteStream::SetDouble(REFGUID guid_key, double value) {
  if (attributes_.Get()) {
    return attributes_->SetDouble(guid_key, value);
  } else {
    return E_FAIL;
  }
}

HRESULT STDMETHODCALLTYPE
WMFByteStream::SetGUID(REFGUID guid_key, REFGUID guid_value) {
  if (attributes_.Get()) {
    return attributes_->SetGUID(guid_key, guid_value);
  } else {
    return E_FAIL;
  }
}

HRESULT STDMETHODCALLTYPE
WMFByteStream::SetString(REFGUID guid_key, LPCWSTR value) {
  if (attributes_.Get()) {
    return attributes_->SetString(guid_key, value);
  } else {
    return E_FAIL;
  }
}

HRESULT STDMETHODCALLTYPE
WMFByteStream::SetBlob(REFGUID guid_key, const UINT8* buf, UINT32 buf_size) {
  if (attributes_.Get()) {
    return attributes_->SetBlob(guid_key, buf, buf_size);
  } else {
    return E_FAIL;
  }
}

HRESULT STDMETHODCALLTYPE
WMFByteStream::SetUnknown(REFGUID guid_key, IUnknown* unknown) {
  if (attributes_.Get()) {
    return attributes_->SetUnknown(guid_key, unknown);
  } else {
    return E_FAIL;
  }
}

HRESULT STDMETHODCALLTYPE WMFByteStream::LockStore() {
  if (attributes_.Get()) {
    return attributes_->LockStore();
  } else {
    return E_FAIL;
  }
}

HRESULT STDMETHODCALLTYPE WMFByteStream::UnlockStore() {
  if (attributes_.Get()) {
    return attributes_->UnlockStore();
  } else {
    return E_FAIL;
  }
}

HRESULT STDMETHODCALLTYPE WMFByteStream::GetCount(UINT32* items) {
  if (attributes_.Get()) {
    return attributes_->GetCount(items);
  } else {
    return E_FAIL;
  }
}

HRESULT STDMETHODCALLTYPE WMFByteStream::GetItemByIndex(UINT32 index,
                                                        GUID* guid_key,
                                                        PROPVARIANT* value) {
  if (attributes_.Get()) {
    return attributes_->GetItemByIndex(index, guid_key, value);
  } else {
    return E_FAIL;
  }
}

HRESULT STDMETHODCALLTYPE WMFByteStream::CopyAllItems(IMFAttributes* dest) {
  if (attributes_.Get()) {
    return attributes_->CopyAllItems(dest);
  } else {
    return E_FAIL;
  }
}

}  // namespace media
