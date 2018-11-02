// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2013 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#include "platform_media/gpu/decoders/win/read_stream.h"
#include "platform_media/gpu/decoders/win/read_stream_listener.h"

#include "base/bind.h"
#include "base/synchronization/waitable_event.h"
#include "media/base/bind_to_current_loop.h"

namespace media {

namespace {

void BlockingReadDone(int* bytes_read_out,
                      base::WaitableEvent* read_done,
                      int bytes_read) {
  *bytes_read_out = bytes_read;
  read_done->Signal();
}

}

ReadStream::ReadStream(DataSource* data_source)
  : data_source_(data_source),
    listener_(nullptr) {
  DCHECK(data_source_);
}

ReadStream::~ReadStream() {}

void ReadStream::Initialize(ReadStreamListener * listener) {
  DCHECK(listener);
  listener_ = listener;
  read_cb_ =
      BindToCurrentLoop(
        base::Bind(&ReadStream::OnReadData, base::Unretained(this)));
}

void ReadStream::Stop() {
  LOG_IF(WARNING, current_read_.Incomplete())
      << " PROPMEDIA(GPU) : " << __FUNCTION__
      << " Stopping while in an Incomplete Read";
  stream_.Stop();
}

bool ReadStream::HasStopped() const {
  return stream_.HasStopped();
}

bool ReadStream::IsStreaming() {
  return data_source_->IsStreaming();
}

int64_t ReadStream::Size() {
  int64_t size = kUnknownSize;
  bool hasSize = data_source_->GetSize(&size);
  return hasSize ? size : kUnknownSize;
}

bool ReadStream::HasSize() {
  return Size() != kUnknownSize;
}

int64_t ReadStream::CurrentPosition() {
  return stream_.CurrentPosition();
}

void ReadStream::SetCurrentPosition(int64_t position) {
  LOG_IF(WARNING, current_read_.Incomplete())
      << " PROPMEDIA(GPU) : " << __FUNCTION__
      << " Setting position while in an Incomplete Read";
  if(current_read_.Incomplete())
    stream_.SetNextPosition(position);
  else
    stream_.SetCurrentPosition(position);
}

bool ReadStream::IsEndOfStream() {
  if(!HasSize() && stream_.HasReceivedEOF())
    return true;
  int64_t size = Size();
  int64_t position = CurrentPosition();
  return (size > 0 && position >= size);
}

int ReadStream::SyncRead(uint8_t* buff, size_t len) {

  LOG_IF(WARNING, current_read_.Incomplete())
      << " PROPMEDIA(GPU) : " << __FUNCTION__
      << " Synchronous Read while in an Incomplete Read";

  base::WaitableEvent read_done(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                                base::WaitableEvent::InitialState::NOT_SIGNALED);
  int bytes_read = 0;
  data_source_->Read(stream_.CurrentPosition(), len, buff,
                     base::Bind(&BlockingReadDone, &bytes_read, &read_done));
  read_done.Wait();
  if (bytes_read == DataSource::kReadError)
    return DataSource::kReadError;

  stream_.ReceivedBytes(bytes_read);
  return bytes_read;
}

void ReadStream::AsyncRead(uint8_t* buff, size_t len) {
  DCHECK(!read_cb_.is_null());
  DCHECK(buff);
  current_read_.Init(buff, len);
  Read();
}

void ReadStream::Read() {
  DCHECK(!read_cb_.is_null());
  data_source_->Read(stream_.CurrentPosition(),
                     current_read_.RemainingBytes(),
                     current_read_.BufferPos(),
                     read_cb_);
}

void ReadStream::FinishRead()
{
  size_t total_num_bytes = current_read_.Total();
  current_read_.Reset();
  stream_.UpdateCurrentPosition();
  listener_->OnReadData(total_num_bytes);
}

void ReadStream::OnReadData(int bytes_read) {

  if(stream_.HasStopped()) {
    LOG(WARNING) << " PROPMEDIA(GPU) : " << __FUNCTION__
                 << " Received on stopped stream bytes " << bytes_read;
    return;
  }

  if(!stream_.ReceivedBytes(bytes_read)) {
    LOG(INFO) << " PROPMEDIA(GPU) : " << __FUNCTION__
              << " No bytes received, assuming end of stream. "
              << " Finishing Incomplete Read, bytes still missing : "
              << current_read_.RemainingBytes();
    FinishRead();
    return;
  }

  current_read_.ReceivedBytes(bytes_read);

  if (current_read_.Incomplete()) {
    DCHECK(current_read_.RemainingBytes() > 0);
    bool is_streaming = IsStreaming();
    bool halfway_done = (current_read_.Total() >= current_read_.RemainingBytes());
    if (!is_streaming || halfway_done) {
      LOG(INFO) << " PROPMEDIA(GPU) : " << __FUNCTION__
                << " Is streaming " << is_streaming
                << " Halfway done " << halfway_done
                << " Reading more on Incomplete Read, bytes missing : "
                << current_read_.RemainingBytes();
      Read();
    } else {
      LOG(INFO) << " PROPMEDIA(GPU) : " << __FUNCTION__
                << " Finishing Incomplete Read, bytes still missing : "
                << current_read_.RemainingBytes();
      FinishRead();
    }
  } else {
    DCHECK(current_read_.RemainingBytes() == 0);
    FinishRead();
  }
}

}
