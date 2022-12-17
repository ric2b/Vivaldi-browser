// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2013 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#include "platform_media/ipc_demuxer/gpu/decoders/win/wmf_byte_stream.h"

#include "platform_media/ipc_demuxer/platform_ipc_util.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/synchronization/waitable_event.h"

#include <algorithm>

namespace media {

namespace {

int CheckReadLength(ULONG length) {
  if (length == 0 || length > static_cast<ULONG>(kMaxSharedMemorySize)) {
    LOG(WARNING) << " PROPMEDIA(GPU) : " << __FUNCTION__
                 << " (E_INVALIDARG) invalid_length length=" << length;
    return -1;
  }
  return static_cast<int>(length);
}

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
  WMFReadRequest(bool is_streaming)
      : is_streaming_(is_streaming),

        // Unretained is safe as the reference count for async_result_ that owns
        // us is manually managed and can only be decreased after the last read
        // is done, see BeginRead below.
        read_cb_(base::BindRepeating(&WMFReadRequest::OnReadData,
                                     base::Unretained(this))) {
    VLOG(5) << " PROPMEDIA(GPU) : " << __FUNCTION__ << " this=" << this;
  }

  ~WMFReadRequest() override {
    VLOG(5) << " PROPMEDIA(GPU) : " << __FUNCTION__ << " this=" << this
            << " initial_position=" << initial_position_
            << " all_read=" << (total_read_ == length_);
  }

  int remaining_bytes() const { return length_ - total_read_; }

  void StartReadOnWorkerThread(
      int64_t initial_position,
      BYTE* memory,
      int length,
      const scoped_refptr<base::SequencedTaskRunner>& main_task_runner,
      ipc_data_source::Buffer source_buffer,
      IMFAsyncResult* async_result) {
    DCHECK_GE(initial_position, 0);
    DCHECK(memory);
    DCHECK(length > 0);
    DCHECK(source_buffer);

    initial_position_ = initial_position;
    memory_ = memory;
    length_ = length;
    received_eos_ = false;
    total_read_ = 0;
    source_buffer_ = std::move(source_buffer);
    async_result_ = async_result;

    // See comments before read_cb_ initializer in the constructor why
    // Unretained is safe.
    main_task_runner->PostTask(FROM_HERE,
                               base::BindOnce(&WMFReadRequest::ReadOnMainThread,
                                              base::Unretained(this)));
  }

  void ReadOnMainThread() {
    VLOG(7) << " PROPMEDIA(GPU) : " << __FUNCTION__ << " this=" << this
            << " initial_position=" << initial_position_
            << " total_read=" << total_read_
            << " remaining_bytes=" << remaining_bytes()
            << " is_streaming=" << is_streaming_;
    if (source_buffer_.IsReadError()) {
      OnReadData(std::move(source_buffer_));
      return;
    }
    DCHECK(source_buffer_);
    int to_read = std::min(remaining_bytes(), source_buffer_.GetCapacity());
    source_buffer_.SetReadRange(initial_position_ + total_read_, to_read);
    ipc_data_source::Buffer::Read(std::move(source_buffer_), read_cb_);
  }

  void OnReadData(ipc_data_source::Buffer source_buffer) {
    // We are called on the main thread here.
    source_buffer_ = std::move(source_buffer);

    HRESULT status = S_OK;
    do {
      int bytes_read = source_buffer_.GetReadSize();
      if (bytes_read < 0) {
        LOG(WARNING) << " PROPMEDIA(GPU) : " << __FUNCTION__
                     << " Read error bytes_read=" << bytes_read
                     << " remaining_bytes=" << remaining_bytes();
        status = E_FAIL;
        break;
      }
      if (bytes_read == 0) {
        received_eos_ = true;
        VLOG(7) << " PROPMEDIA(GPU) : " << __FUNCTION__ << " received_eos"
                << " position=" << initial_position_
                << " total_read=" << total_read_
                << " remaining_bytes=" << remaining_bytes();
        if (total_read_ == 0) {
          // Report an empty read as an error.
          status = E_INVALIDARG;
        }
        break;
      }

      // Ensure that the caller did all the memory safety checks.
      CHECK(bytes_read <= remaining_bytes());
      memcpy(memory_ + total_read_, source_buffer_.GetReadData(), bytes_read);
      total_read_ += bytes_read;

      if (total_read_ == length_)
        break;

      if (is_streaming_) {
        bool halfway_done = (total_read_ >= remaining_bytes());
        if (!halfway_done) {
          VLOG(7) << " PROPMEDIA(GPU) : " << __FUNCTION__
                  << " Finishing Incomplete Read, bytes still missing : "
                  << remaining_bytes();
          break;
        }
      }

      ReadOnMainThread();
      return;

    } while (false);

    async_result_->SetStatus(status);
    MFInvokeCallback(async_result_);
  }

  int64_t initial_position_ = 0;
  BYTE* memory_ = nullptr;
  int length_ = 0;
  const bool is_streaming_;
  bool received_eos_ = false;
  int total_read_ = 0;

  // the owner
  IMFAsyncResult* async_result_ = nullptr;
  ipc_data_source::Buffer source_buffer_;
  const base::RepeatingCallback<void(ipc_data_source::Buffer)> read_cb_;
};

}  // namespace

WMFByteStream::WMFByteStream() {
  VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__ << " this=" << this;
}

WMFByteStream::~WMFByteStream() {
  VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__ << " this=" << this;
}

void WMFByteStream::Initialize(
    scoped_refptr<base::SequencedTaskRunner> main_task_runner,
    ipc_data_source::Buffer source_buffer,
    bool is_streaming,
    int64_t stream_length) {
  VLOG(2) << " PROPMEDIA(GPU) : " << __FUNCTION__
          << " stream_length=" << stream_length
          << " is_streaming=" << is_streaming;
  DCHECK(main_task_runner);
  DCHECK(source_buffer);

  main_task_runner_ = std::move(main_task_runner);
  source_buffer_ = std::move(source_buffer);

  // The Media Framework expects exactly -1 when the size is unknown.
  stream_length_ = std::max(stream_length, -1LL);
  is_streaming_ = is_streaming;
}

HRESULT STDMETHODCALLTYPE WMFByteStream::GetCapabilities(DWORD* capabilities) {
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

HRESULT STDMETHODCALLTYPE WMFByteStream::GetCurrentPosition(QWORD* position) {
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
    VLOG(2) << " PROPMEDIA(GPU) : " << __FUNCTION__
            << " Cannot SetCurrentPosition to " << position
            << " Media is streaming";
  } else {
    VLOG(7) << " PROPMEDIA(GPU) : " << __FUNCTION__ << " SetCurrentPosition "
            << position;
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

HRESULT STDMETHODCALLTYPE WMFByteStream::Read(BYTE* buff,
                                              ULONG len,
                                              ULONG* read) {
  *read = 0;
  int max_read = CheckReadLength(len);
  if (max_read < 0)
    return E_INVALIDARG;

  if (!source_buffer_) {
    LOG(WARNING) << " PROPMEDIA(GPU) : " << __FUNCTION__
                 << " (E_FAIL) Attempt to read while another read is pending";
    return E_FAIL;
  }
  if (source_buffer_.IsReadError()) {
    VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
            << " (E_FAIL) Attempt to read already failed buffer";
    return E_FAIL;
  }

  source_buffer_.SetReadRange(stream_position_,
                              std::min(max_read, source_buffer_.GetCapacity()));

  auto blocking_read_done = [](ipc_data_source::Buffer* buffer_ptr,
                               base::WaitableEvent* read_done,
                               ipc_data_source::Buffer buffer) {
    *buffer_ptr = std::move(buffer);
    read_done->Signal();
  };

  // source_buffer_ must be modified only on the worker thread. So use a
  // temporary to receive the buffer with the result on the main thread while
  // this thread waits below for read_done signal.
  ipc_data_source::Buffer result_buffer;
  base::WaitableEvent read_done(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &ipc_data_source::Buffer::Read, std::move(source_buffer_),
          base::BindOnce(blocking_read_done, &result_buffer, &read_done)));

  VLOG(7) << " PROPMEDIA(GPU) : " << __FUNCTION__ << " Start blocking read";

  // Wait until the callback is called from the main thread.
  read_done.Wait();

  VLOG(7) << " PROPMEDIA(GPU) : " << __FUNCTION__ << " End blocking read";
  source_buffer_ = std::move(result_buffer);
  int bytes_read = source_buffer_.GetReadSize();
  if (bytes_read < 0) {
    LOG(WARNING) << " PROPMEDIA(GPU) : " << __FUNCTION__
                 << " (E_FAIL) Stream sync read error bytes_read="
                 << bytes_read;
    return E_FAIL;
  }

  if (bytes_read == 0) {
    LOG(INFO) << " PROPMEDIA(GPU) : " << __FUNCTION__
              << " no_data_read received_eos"
              << " remaining_bytes=" << len;
    received_eos_ = true;
  } else {
    CHECK(bytes_read <= max_read);
    memcpy(buff, source_buffer_.GetReadData(), bytes_read);
    stream_position_ += bytes_read;
  }
  *read = static_cast<ULONG>(bytes_read);
  return S_OK;
}

HRESULT STDMETHODCALLTYPE WMFByteStream::BeginRead(BYTE* buff,
                                                   ULONG len,
                                                   IMFAsyncCallback* callback,
                                                   IUnknown* state) {
  VLOG(4) << " PROPMEDIA(GPU) : " << __FUNCTION__ << " len: " << len;
  int max_read = CheckReadLength(len);
  if (max_read < 0)
    return E_INVALIDARG;
  if (!source_buffer_) {
    LOG(WARNING) << " PROPMEDIA(GPU) : " << __FUNCTION__
                 << " (E_FAIL) Attempt to read while another read is pending";
    return E_FAIL;
  }

  Microsoft::WRL::ComPtr<WMFReadRequest> read_request(
      Microsoft::WRL::Make<WMFReadRequest>(is_streaming_));

  // async_result is released in EndRead.
  IMFAsyncResult* async_result = nullptr;
  HRESULT hresult =
      MFCreateAsyncResult(read_request.Get(), callback, state, &async_result);
  if (FAILED(hresult)) {
    LOG(WARNING) << " PROPMEDIA(GPU) : " << __FUNCTION__
                 << " (E_ABORT) MFCreateAsyncResult failed";
    return E_ABORT;
  }

  read_request->StartReadOnWorkerThread(
      stream_position_, buff, max_read, main_task_runner_,
      std::move(source_buffer_), async_result);
  return S_OK;
}

HRESULT STDMETHODCALLTYPE WMFByteStream::EndRead(IMFAsyncResult* result,
                                                 ULONG* read) {
  HRESULT hresult;
  Microsoft::WRL::ComPtr<IUnknown> unknown;

  // Do not recover from memory errors.
  CHECK(SUCCEEDED(result->GetObjectW(unknown.GetAddressOf())) && unknown);

  WMFReadRequest* read_request = static_cast<WMFReadRequest*>(unknown.Get());
  source_buffer_ = std::move(read_request->source_buffer_);
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

  // was acquired by a call to MFCreateAsyncResult in BeginRead
  result->Release();
  return hresult;
}

HRESULT STDMETHODCALLTYPE WMFByteStream::Write(const BYTE* buff,
                                               ULONG len,
                                               ULONG* written) {
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

HRESULT STDMETHODCALLTYPE WMFByteStream::EndWrite(IMFAsyncResult* result,
                                                  ULONG* written) {
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

      VLOG(7) << " PROPMEDIA(GPU) : " << __FUNCTION__ << " SetCurrentPosition "
              << seek_offset;
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

      VLOG(7) << " PROPMEDIA(GPU) : " << __FUNCTION__ << " SetCurrentPosition "
              << next_position;
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
  VLOG(2) << " PROPMEDIA(GPU) : " << __FUNCTION__;
  main_task_runner_.reset();

  // Move to temporary that will be destructed.
  ipc_data_source::Buffer to_release(std::move(source_buffer_));
  return S_OK;
}

}  // namespace media
