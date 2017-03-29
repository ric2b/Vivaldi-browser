// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (C) 2013 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#include "content/common/gpu/media/wmf_byte_stream.h"

#include <algorithm>

#include "base/bind.h"
#include "base/synchronization/waitable_event.h"
#include "content/common/gpu/media/wmf_media_pipeline.h"
#include "media/base/bind_to_current_loop.h"

namespace content {

namespace {

class WMFReadRequest : public base::win::IUnknownImpl {
 public:
  WMFReadRequest(BYTE* buff, ULONG len) : buffer(buff), length(len), read(0) {
  }

  ~WMFReadRequest() {}

  BYTE* buffer;
  ULONG length;
  ULONG read;
};

void BlockingReadDone(int* bytes_read_out,
                      base::WaitableEvent* read_done,
                      int bytes_read) {
  *bytes_read_out = bytes_read;
  read_done->Signal();
}

}  // namespace

const int64 WMFByteStream::kUnknownSize = -1;

WMFByteStream::WMFByteStream(media::DataSource* data_source)
    : data_source_(data_source),
      async_result_(NULL),
      last_read_bytes_(0),
      read_position_(0),
      stopped_(false),
      ref_count_(0),
      weak_factory_(this) {
  DCHECK(data_source_);
}

WMFByteStream::~WMFByteStream() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(stopped_);
  DCHECK(base::AtomicRefCountIsZero(&ref_count_));
}

HRESULT WMFByteStream::Initialize(LPCWSTR mime_type) {
  DCHECK(thread_checker_.CalledOnValidThread());

  HRESULT hresult = MFCreateAttributes(attributes_.Receive(), 1);
  if (SUCCEEDED(hresult)) {
    attributes_->SetString(MF_BYTESTREAM_CONTENT_TYPE, mime_type);
    read_cb_ = media::BindToCurrentLoop(
        base::Bind(&WMFByteStream::OnReadData, weak_factory_.GetWeakPtr()));
    return S_OK;
  }

  return E_ABORT;
}

void WMFByteStream::Stop() {
  DCHECK(thread_checker_.CalledOnValidThread());

  stopped_ = true;

  if (async_result_) {
    // Set async_result_ to NULL before calling MFInvokeCallback. Doing
    // it after that call creates race condition because BeginRead may
    // be called before this function returns. Callback will pass result
    // to EndProc where it will be released.
    IMFAsyncResult* result = async_result_;
    async_result_ = NULL;
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
#if DCHECK_IS_ON()
  base::AtomicRefCountInc(&ref_count_);
#endif
  return 1;
}

ULONG STDMETHODCALLTYPE WMFByteStream::Release() {
#if DCHECK_IS_ON()
  base::AtomicRefCountDec(&ref_count_);
#endif
  return 1;
}

HRESULT STDMETHODCALLTYPE
WMFByteStream::GetCapabilities(DWORD* capabilities) {
  *capabilities =
      MFBYTESTREAM_IS_READABLE | MFBYTESTREAM_IS_SEEKABLE |
      (data_source_->IsStreaming()
           ? (MFBYTESTREAM_HAS_SLOW_SEEK | MFBYTESTREAM_IS_PARTIALLY_DOWNLOADED)
           : 0);

  return S_OK;
}

HRESULT STDMETHODCALLTYPE WMFByteStream::GetLength(QWORD* length) {
  int64 size = -1;
  // The Media Framework expects -1 when the size is unknown.
  *length = data_source_->GetSize(&size) ? size : kUnknownSize;
  return S_OK;
}

HRESULT STDMETHODCALLTYPE WMFByteStream::SetLength(QWORD length) {
  // The stream is not writable, so do nothing here.
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE
WMFByteStream::GetCurrentPosition(QWORD* position) {
  *position = read_position_;
  return S_OK;
}

HRESULT STDMETHODCALLTYPE WMFByteStream::SetCurrentPosition(QWORD position) {
  if (static_cast<int64>(position) < 0) {
    return E_INVALIDARG;  // might happen if the stream is not seekable or
                          // if the position overflows the stream
  }

  read_position_ = position;
  return S_OK;
}

HRESULT STDMETHODCALLTYPE WMFByteStream::IsEndOfStream(BOOL* end_of_stream) {
  int64 size = -1;
  if (!data_source_->GetSize(&size))
    size = kUnknownSize;
  *end_of_stream = size > 0 && read_position_ >= size;
  return S_OK;
}

HRESULT STDMETHODCALLTYPE
WMFByteStream::Read(BYTE* buff, ULONG len, ULONG* read) {
  DCHECK(!thread_checker_.CalledOnValidThread())
      << "Trying to make a blocking read on the main thread";

  base::WaitableEvent read_done(false, false);
  int bytes_read = 0;
  data_source_->Read(read_position_, len, buff,
                     base::Bind(&BlockingReadDone, &bytes_read, &read_done));
  read_done.Wait();
  if (bytes_read == media::DataSource::kReadError)
    return E_FAIL;

  read_position_ += bytes_read;
  *read = bytes_read;

  return S_OK;
}

void WMFByteStream::OnReadData(int size) {
  DCHECK(thread_checker_.CalledOnValidThread());

  base::win::ScopedComPtr<IUnknown> unknown;
  HRESULT hr = async_result_->GetObjectW(unknown.Receive());
  WMFReadRequest* read_request = static_cast<WMFReadRequest*>(unknown.get());

  HRESULT status;
  if (FAILED(hr) || !unknown || size == media::DataSource::kReadError) {
    status = E_FAIL;
  } else if (stopped_) {
    status = E_INVALIDARG;
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
  DCHECK(!read_cb_.is_null());

  if (stopped_) {
    return E_INVALIDARG;
  }

  if (async_result_) {
    return E_ABORT;
  }

  base::win::ScopedComPtr<IUnknown> read_request(new WMFReadRequest(buff, len));
  HRESULT hresult =
      MFCreateAsyncResult(read_request.get(), callback, state, &async_result_);
  if (FAILED(hresult))
    return E_ABORT;

  data_source_->Read(read_position_, len, buff, read_cb_);
  read_position_ += len;
  return S_OK;
}

HRESULT STDMETHODCALLTYPE
WMFByteStream::EndRead(IMFAsyncResult* result, ULONG* read) {
  HRESULT hresult;
  base::win::ScopedComPtr<IUnknown> unknown;
  if (FAILED(result->GetObjectW(unknown.Receive())) || !unknown) {
    hresult = E_INVALIDARG;
  } else {
    WMFReadRequest* read_request = static_cast<WMFReadRequest*>(unknown.get());
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
  int64 size = -1;
  if (!data_source_->GetSize(&size))
    size = kUnknownSize;
  switch (seek_origin) {
    case msoBegin:
      if ((size > 0 && seek_offset > size) || seek_offset < 0) {
        return E_INVALIDARG;  // might happen if the stream is not seekable or
                              // if the llSeekOffset overflows the stream
      }
      read_position_ = seek_offset;
      break;

    case msoCurrent:
      if ((size > 0 && (read_position_ + seek_offset) > size) ||
          (read_position_ + seek_offset) < 0) {
        return E_INVALIDARG;  // might happen if the stream is not seekable or
                              // if the llSeekOffset overflows the stream
      }
      read_position_ += seek_offset;
      break;
  }

  *current_position = read_position_;
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
  if (attributes_.get()) {
    return attributes_->GetItem(guid_key, value);
  } else {
    return E_FAIL;
  }
}

HRESULT STDMETHODCALLTYPE
WMFByteStream::GetItemType(REFGUID guid_key, MF_ATTRIBUTE_TYPE* type) {
  if (attributes_.get()) {
    return attributes_->GetItemType(guid_key, type);
  } else {
    return E_FAIL;
  }
}

HRESULT STDMETHODCALLTYPE WMFByteStream::CompareItem(REFGUID guid_key,
                                                     REFPROPVARIANT value,
                                                     BOOL* result) {
  if (attributes_.get()) {
    return attributes_->CompareItem(guid_key, value, result);
  } else {
    return E_FAIL;
  }
}

HRESULT STDMETHODCALLTYPE
WMFByteStream::Compare(IMFAttributes* theirs,
                       MF_ATTRIBUTES_MATCH_TYPE match_type,
                       BOOL* result) {
  if (attributes_.get()) {
    return attributes_->Compare(theirs, match_type, result);
  } else {
    return E_FAIL;
  }
}

HRESULT STDMETHODCALLTYPE
WMFByteStream::GetUINT32(REFGUID guid_key, UINT32* value) {
  if (attributes_.get()) {
    return attributes_->GetUINT32(guid_key, value);
  } else {
    return E_FAIL;
  }
}

HRESULT STDMETHODCALLTYPE
WMFByteStream::GetUINT64(REFGUID guid_key, UINT64* value) {
  if (attributes_.get()) {
    return attributes_->GetUINT64(guid_key, value);
  } else {
    return E_FAIL;
  }
}

HRESULT STDMETHODCALLTYPE
WMFByteStream::GetDouble(REFGUID guid_key, double* value) {
  if (attributes_.get()) {
    return attributes_->GetDouble(guid_key, value);
  } else {
    return E_FAIL;
  }
}

HRESULT STDMETHODCALLTYPE
WMFByteStream::GetGUID(REFGUID guid_key, GUID* guid_value) {
  if (attributes_.get()) {
    return attributes_->GetGUID(guid_key, guid_value);
  } else {
    return E_FAIL;
  }
}

HRESULT STDMETHODCALLTYPE
WMFByteStream::GetStringLength(REFGUID guid_key, UINT32* length) {
  if (attributes_.get()) {
    return attributes_->GetStringLength(guid_key, length);
  } else {
    return E_FAIL;
  }
}

HRESULT STDMETHODCALLTYPE WMFByteStream::GetString(REFGUID guid_key,
                                                   LPWSTR value,
                                                   UINT32 buf_size,
                                                   UINT32* length) {
  if (attributes_.get()) {
    return attributes_->GetString(guid_key, value, buf_size, length);
  } else {
    return E_FAIL;
  }
}

HRESULT STDMETHODCALLTYPE
WMFByteStream::GetAllocatedString(REFGUID guid_key,
                                  LPWSTR* value,
                                  UINT32* length) {
  if (attributes_.get()) {
    return attributes_->GetAllocatedString(guid_key, value, length);
  } else {
    return E_FAIL;
  }
}

HRESULT STDMETHODCALLTYPE
WMFByteStream::GetBlobSize(REFGUID guid_key, UINT32* blob_size) {
  if (attributes_.get()) {
    return attributes_->GetBlobSize(guid_key, blob_size);
  } else {
    return E_FAIL;
  }
}

HRESULT STDMETHODCALLTYPE WMFByteStream::GetBlob(REFGUID guid_key,
                                                 UINT8* buf,
                                                 UINT32 buf_size,
                                                 UINT32* blob_size) {
  if (attributes_.get()) {
    return attributes_->GetBlob(guid_key, buf, buf_size, blob_size);
  } else {
    return E_FAIL;
  }
}

HRESULT STDMETHODCALLTYPE WMFByteStream::GetAllocatedBlob(REFGUID guid_key,
                                                          UINT8** buf,
                                                          UINT32* size) {
  if (attributes_.get()) {
    return attributes_->GetAllocatedBlob(guid_key, buf, size);
  } else {
    return E_FAIL;
  }
}

HRESULT STDMETHODCALLTYPE
WMFByteStream::GetUnknown(REFGUID guid_key, REFIID riid, LPVOID* ppv) {
  if (attributes_.get()) {
    return attributes_->GetUnknown(guid_key, riid, ppv);
  } else {
    return E_FAIL;
  }
}

HRESULT STDMETHODCALLTYPE
WMFByteStream::SetItem(REFGUID guid_key, REFPROPVARIANT value) {
  if (attributes_.get()) {
    return attributes_->SetItem(guid_key, value);
  } else {
    return E_FAIL;
  }
}

HRESULT STDMETHODCALLTYPE WMFByteStream::DeleteItem(REFGUID guid_key) {
  if (attributes_.get()) {
    return attributes_->DeleteItem(guid_key);
  } else {
    return E_FAIL;
  }
}

HRESULT STDMETHODCALLTYPE WMFByteStream::DeleteAllItems() {
  if (attributes_.get()) {
    return attributes_->DeleteAllItems();
  } else {
    return E_FAIL;
  }
}

HRESULT STDMETHODCALLTYPE
WMFByteStream::SetUINT32(REFGUID guid_key, UINT32 value) {
  if (attributes_.get()) {
    return attributes_->SetUINT32(guid_key, value);
  } else {
    return E_FAIL;
  }
}

HRESULT STDMETHODCALLTYPE
WMFByteStream::SetUINT64(REFGUID guid_key, UINT64 value) {
  if (attributes_.get()) {
    return attributes_->SetUINT64(guid_key, value);
  } else {
    return E_FAIL;
  }
}

HRESULT STDMETHODCALLTYPE
WMFByteStream::SetDouble(REFGUID guid_key, double value) {
  if (attributes_.get()) {
    return attributes_->SetDouble(guid_key, value);
  } else {
    return E_FAIL;
  }
}

HRESULT STDMETHODCALLTYPE
WMFByteStream::SetGUID(REFGUID guid_key, REFGUID guid_value) {
  if (attributes_.get()) {
    return attributes_->SetGUID(guid_key, guid_value);
  } else {
    return E_FAIL;
  }
}

HRESULT STDMETHODCALLTYPE
WMFByteStream::SetString(REFGUID guid_key, LPCWSTR value) {
  if (attributes_.get()) {
    return attributes_->SetString(guid_key, value);
  } else {
    return E_FAIL;
  }
}

HRESULT STDMETHODCALLTYPE
WMFByteStream::SetBlob(REFGUID guid_key, const UINT8* buf, UINT32 buf_size) {
  if (attributes_.get()) {
    return attributes_->SetBlob(guid_key, buf, buf_size);
  } else {
    return E_FAIL;
  }
}

HRESULT STDMETHODCALLTYPE
WMFByteStream::SetUnknown(REFGUID guid_key, IUnknown* unknown) {
  if (attributes_.get()) {
    return attributes_->SetUnknown(guid_key, unknown);
  } else {
    return E_FAIL;
  }
}

HRESULT STDMETHODCALLTYPE WMFByteStream::LockStore() {
  if (attributes_.get()) {
    return attributes_->LockStore();
  } else {
    return E_FAIL;
  }
}

HRESULT STDMETHODCALLTYPE WMFByteStream::UnlockStore() {
  if (attributes_.get()) {
    return attributes_->UnlockStore();
  } else {
    return E_FAIL;
  }
}

HRESULT STDMETHODCALLTYPE WMFByteStream::GetCount(UINT32* items) {
  if (attributes_.get()) {
    return attributes_->GetCount(items);
  } else {
    return E_FAIL;
  }
}

HRESULT STDMETHODCALLTYPE WMFByteStream::GetItemByIndex(UINT32 index,
                                                        GUID* guid_key,
                                                        PROPVARIANT* value) {
  if (attributes_.get()) {
    return attributes_->GetItemByIndex(index, guid_key, value);
  } else {
    return E_FAIL;
  }
}

HRESULT STDMETHODCALLTYPE WMFByteStream::CopyAllItems(IMFAttributes* dest) {
  if (attributes_.get()) {
    return attributes_->CopyAllItems(dest);
  } else {
    return E_FAIL;
  }
}

}  // namespace content
