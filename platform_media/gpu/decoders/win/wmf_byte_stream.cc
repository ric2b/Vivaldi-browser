// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2013 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#include "platform_media/gpu/decoders/win/wmf_byte_stream.h"

#include "platform_media/common/platform_ipc_util.h"

#include "base/bind.h"
#include "base/synchronization/waitable_event.h"

#include <algorithm>

namespace media {


WMFByteStream::WMFByteStream() {
  VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__ << " this=" << this;
}

WMFByteStream::~WMFByteStream() {
  VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__ << " this=" << this;
}

void WMFByteStream::Initialize(
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    ipc_data_source::Reader source_reader,
    bool is_streaming,
    int64_t stream_length) {
  VLOG(4) << " PROPMEDIA(GPU) : " << __FUNCTION__
          << " stream_length=" << stream_length
          << " is_streaming=" << is_streaming;

  main_task_runner_ = std::move(main_task_runner);
  source_reader_ = std::move(source_reader);

  // The Media Framework expects exactly -1 when the size is unknown.
  stream_length_ = std::max(stream_length, -1LL);
  is_streaming_ = is_streaming;
}

HRESULT STDMETHODCALLTYPE
WMFByteStream::GetCapabilities(DWORD* capabilities) {
  *capabilities =
      MFBYTESTREAM_IS_READABLE | MFBYTESTREAM_IS_SEEKABLE |
      (is_streaming_
           ? (MFBYTESTREAM_HAS_SLOW_SEEK | MFBYTESTREAM_IS_PARTIALLY_DOWNLOADED)
           : 0);

  return S_OK;
}

HRESULT STDMETHODCALLTYPE WMFByteStream::GetLength(QWORD* length) {
  *length = stream_length_;
  return S_OK;
}

HRESULT STDMETHODCALLTYPE WMFByteStream::SetLength(QWORD length) {
  // The stream is not writable, so do nothing here.
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE
WMFByteStream::GetCurrentPosition(QWORD* position) {
  *position = stream_position_;
  return S_OK;
}

HRESULT STDMETHODCALLTYPE WMFByteStream::SetCurrentPosition(QWORD position) {
  if (static_cast<int64_t>(position) < 0) {
    LOG(WARNING) << " PROPMEDIA(GPU) : " << __FUNCTION__
                 << " (E_INVALIDARG) Invalid position";
    return E_INVALIDARG;  // might happen if the stream is not seekable or
                          // if the position overflows the stream
  }

  if (is_streaming_) {
    VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
            << " Cannot SetCurrentPosition to " << position
            << " Media is streaming";
  }
  else
  {
    VLOG(7) << " PROPMEDIA(GPU) : " << __FUNCTION__
            << " SetCurrentPosition " << position;
    stream_position_ = position;
  }
  return S_OK;
}

HRESULT STDMETHODCALLTYPE WMFByteStream::IsEndOfStream(BOOL* end_of_stream) {
  if (stream_length_ < 0) {
    *end_of_stream = received_eos_;
  } else {
    *end_of_stream = (stream_position_ >= stream_length_);
  }
  return S_OK;
}

namespace {

int CheckReadLength(ULONG length) {
  if (length == 0 || length > static_cast<ULONG>(kMaxSharedMemorySize)) {
    LOG(WARNING) << " PROPMEDIA(GPU) : " << __FUNCTION__
                 << " (E_INVALIDARG) invalid_length length=" << length;
    return -1;
  }
  return static_cast<int>(length);
}

void BlockingReadDone(BYTE* buff,
                      int max_read,
                      int* bytes_read_out,
                      base::WaitableEvent* read_done,
                      int bytes_read,
                      const uint8_t* data) {
  if (bytes_read > 0) {
    // Ensure that the caller did all memory checks.
    CHECK(bytes_read <= max_read);
    memcpy(buff, data, bytes_read);
  }
  *bytes_read_out = bytes_read;
  read_done->Signal();
}

}  // namespace

HRESULT STDMETHODCALLTYPE WMFByteStream::Read(BYTE* buff,
                                              ULONG len,
                                              ULONG* read) {
  int max_read = CheckReadLength(len);
  if (max_read < 0)
      return E_INVALIDARG;

  base::WaitableEvent read_done(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  int bytes_read = 0;

  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(source_reader_, stream_position_, max_read,
                     base::BindOnce(&BlockingReadDone, buff, max_read,
                                    &bytes_read, &read_done)));

  // Wait until the callback is called from the main thread.
  read_done.Wait();
  if (bytes_read < 0) {
    LOG(WARNING) << " PROPMEDIA(GPU) : " << __FUNCTION__
                 << " (E_FAIL) Stream sync read error bytes_read="
                 << bytes_read;
    *read = 0;
    return E_FAIL;
  }

  if (bytes_read == 0) {
    LOG(INFO) << " PROPMEDIA(GPU) : " << __FUNCTION__
              << " no_data_read received_eos"
              << " remaining_bytes=" << len;
    received_eos_ = true;
  }
  stream_position_ += bytes_read;
  *read = static_cast<ULONG>(bytes_read);
  return S_OK;
}

namespace {

// Helper to hold a temporary state during AsyncRead. It copies enough
// information from a WMFByteStream instance so it can run repeated read
// attempts from the main thread without changing anything in the instance. Then
// in EndRead when we are back to the worker thread we copy updated values back
// to the instance.
class WMFReadRequest
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IUnknown> {
 public:
  WMFReadRequest(ipc_data_source::Reader source_reader,
                 int64_t position,
                 BYTE* buffer,
                 int length,
                 bool is_streaming)
      : source_reader_(std::move(source_reader)),
        initial_position_(position),
        buffer_(buffer),
        length_(length),
        is_streaming_(is_streaming),
        received_eos_(false),
        total_read_(0) {
    DCHECK(length > 0);
  }

  ~WMFReadRequest() override {
    VLOG(4) << " PROPMEDIA(GPU) : " << __FUNCTION__ << " this=" << this
            << " initial_position=" << initial_position_
            << " all_read=" << (total_read_ == length_);
  }

  int remaining_bytes() const { return length_ - total_read_; }

  static void OnReadData(IMFAsyncResult* async_result,
                         int size,
                         const uint8_t* data);

  void StartReadOnWorkerThread(
      const scoped_refptr<base::SingleThreadTaskRunner>& main_task_runner,
      IMFAsyncResult* async_result) {
    DCHECK(total_read_ == 0);
    VLOG(4) << " PROPMEDIA(GPU) : " << __FUNCTION__ << " this=" << this
            << " initial_position=" << initial_position_
            << " remaining_bytes=" << remaining_bytes()
            << " is_streaming=" << is_streaming_;
    // Unretained is safe as the reference count for async_result is manually
    // managed, see BeginRead below.
    main_task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(source_reader_, initial_position_, length_,
                       base::BindOnce(&WMFReadRequest::OnReadData,
                                      base::Unretained(async_result))));
  }

 void ContinueReadOnMainThread(IMFAsyncResult* async_result) {
   VLOG(4) << " PROPMEDIA(GPU) : " << __FUNCTION__ << " this=" << this
           << " initial_position=" << initial_position_
           << " total_read=" << total_read_
           << " remaining_bytes=" << remaining_bytes()
           << " is_streaming=" << is_streaming_;
   // Unretained is safe as the reference count for async_result is manually
   // managed, see BeginRead below.
   source_reader_.Run(initial_position_ + total_read_, remaining_bytes(),
                      base::BindOnce(&WMFReadRequest::OnReadData,
                                     base::Unretained(async_result)));
  }

  ipc_data_source::Reader source_reader_;
  const int64_t initial_position_;
  BYTE* const buffer_;
  const int length_;
  const bool is_streaming_;
  bool received_eos_;
  int total_read_;
};

// static
void WMFReadRequest::OnReadData(IMFAsyncResult* async_result,
                                int bytes_read,
                                const uint8_t* data) {
  // We are called on the main thread here.
  DCHECK(async_result);

  HRESULT status = S_OK;
  do {
    Microsoft::WRL::ComPtr<IUnknown> unknown;
    HRESULT hr = async_result->GetObjectW(unknown.GetAddressOf());
    WMFReadRequest* read_request = static_cast<WMFReadRequest*>(unknown.Get());

    if (FAILED(hr) || bytes_read < 0 || !read_request) {
      LOG(WARNING) << " PROPMEDIA(GPU) : " << __FUNCTION__
                   << " read_error=" << bytes_read
                   << " remaining_bytes=" << read_request->remaining_bytes();
      status = E_FAIL;
      break;
    }
    if (bytes_read == 0) {
      read_request->received_eos_ = true;
      VLOG(2) << " PROPMEDIA(GPU) : " << __FUNCTION__ << " received_eos"
              << " position=" << read_request->initial_position_
              << " total_read=" << read_request->total_read_
              << " remaining_bytes=" << read_request->remaining_bytes();
      if (read_request->total_read_ == 0) {
        // Report an empty read as an error.
        status = E_INVALIDARG;
      }
      break;
    }

    // Ensure that the caller did all the memory safety checks.
    CHECK(bytes_read <= read_request->remaining_bytes());
    memcpy(read_request->buffer_ + read_request->total_read_, data, bytes_read);
    read_request->total_read_ += bytes_read;

    if (read_request->total_read_ == read_request->length_)
      break;

    if (read_request->is_streaming_) {
      bool halfway_done =
          (read_request->total_read_ >= read_request->remaining_bytes());
      if (!halfway_done) {
        VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
                << " Finishing Incomplete Read, bytes still missing : "
                << read_request->remaining_bytes();
        break;
      }
    }

    read_request->ContinueReadOnMainThread(async_result);
    return;
  } while (false);

  async_result->SetStatus(status);
  MFInvokeCallback(async_result);
}

}  // namespace

HRESULT STDMETHODCALLTYPE WMFByteStream::BeginRead(BYTE* buff,
                                                   ULONG len,
                                                   IMFAsyncCallback* callback,
                                                   IUnknown* state) {
  VLOG(4) << " PROPMEDIA(GPU) : " << __FUNCTION__ << " len: " << len;
  int max_read = CheckReadLength(len);
  if (max_read < 0)
      return E_INVALIDARG;

  Microsoft::WRL::ComPtr<WMFReadRequest> read_request(
      Microsoft::WRL::Make<WMFReadRequest>(source_reader_,
                                           stream_position_,
                                           buff, max_read, is_streaming_));

  // async_result is released in EndRead.
  IMFAsyncResult* async_result = nullptr;
  HRESULT hresult =
      MFCreateAsyncResult(read_request.Get(), callback, state, &async_result);
  if (FAILED(hresult)) {
    LOG(WARNING) << " PROPMEDIA(GPU) : " << __FUNCTION__
                 << " (E_ABORT) MFCreateAsyncResult failed";
    return E_ABORT;
  }

  read_request->StartReadOnWorkerThread(main_task_runner_, async_result);
  return S_OK;
}

HRESULT STDMETHODCALLTYPE
WMFByteStream::EndRead(IMFAsyncResult* result, ULONG* read) {
  HRESULT hresult;
  Microsoft::WRL::ComPtr<IUnknown> unknown;
  if (FAILED(result->GetObjectW(unknown.GetAddressOf())) || !unknown) {
    LOG(WARNING) << " PROPMEDIA(GPU) : " << __FUNCTION__
                 << " (E_INVALIDARG) Stream has failed";
    *read = 0;
    hresult = E_INVALIDARG;
  } else {
    WMFReadRequest* read_request = static_cast<WMFReadRequest*>(unknown.Get());
    *read = read_request->total_read_;
    stream_position_ =
        read_request->initial_position_ + read_request->total_read_;
    if (read_request->received_eos_) {
      received_eos_ = true;
    }
    hresult = result->GetStatus();
    VLOG(4) << " PROPMEDIA(GPU) : " << __FUNCTION__
            << " initial_position=" << read_request->initial_position_
            << " all_read="
            << (read_request->total_read_ == read_request->length_)
            << " total_read=" << read_request->total_read_
            << " remaining_bytes=" << read_request->remaining_bytes()
            << " received_eos_=" << read_request->received_eos_
            << " is_streaming=" << is_streaming_ << " hresult=" << hresult;
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
  switch (seek_origin) {
    case msoBegin:
      if ((stream_length_ >= 0 && seek_offset > stream_length_) ||
          seek_offset < 0) {
        LOG(WARNING) << " PROPMEDIA(GPU) : " << __FUNCTION__
                     << " (E_INVALIDARG) Invalid Seek";
        return E_INVALIDARG;  // might happen if the stream is not seekable or
                              // if the llSeekOffset overflows the stream
      }

      VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
              << " SetCurrentPosition " << seek_offset;
      stream_position_ = seek_offset;
      break;

    case msoCurrent:
      int64_t next_position = stream_position_ + seek_offset;
      if ((stream_length_ >= 0 && next_position > stream_length_) ||
          next_position < 0) {
        LOG(WARNING) << " PROPMEDIA(GPU) : " << __FUNCTION__
                     << " (E_INVALIDARG) Invalid Seek";
        return E_INVALIDARG;  // might happen if the stream is not seekable or
                              // if the llSeekOffset overflows the stream
      }

      VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
              << " SetCurrentPosition " << next_position;
      stream_position_ = next_position;
      break;
  }

  *current_position = stream_position_;
  return S_OK;
}

HRESULT STDMETHODCALLTYPE WMFByteStream::Flush() {
  // The stream is not writable, so do nothing here.
  return S_OK;
}

HRESULT STDMETHODCALLTYPE WMFByteStream::Close() {
  return S_OK;
}

}  // namespace media
