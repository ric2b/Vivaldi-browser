// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA.

#include "platform_media/gpu/decoders/mac/data_request_handler.h"

#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "platform_media/gpu/data_source/ipc_data_source.h"

namespace media {

DataRequestHandler::Request::Request() = default;

DataRequestHandler::Request::Request(id request_handle,
                                     int64_t offset,
                                     int64_t length)
    : handle([request_handle retain]), offset(offset), length(length) {
  DCHECK_GE(offset, 0);
  DCHECK(length == -1 || length > 0);
}

DataRequestHandler::Request::Request(Request&& request) = default;

DataRequestHandler::Request::~Request() {
  // The request handle must be moved out at this point
  CHECK(!handle);
}

// This cannot be default as scoped_nsobject does not provide move assignment
// operator.
DataRequestHandler::Request& DataRequestHandler::Request::operator=(
    Request&& request) {
  handle.reset();
  swap(handle, request.handle);
  offset = request.offset;
  length = request.length;
  return *this;
}

DataRequestHandler::OrderedRequests::OrderedRequests() = default;
DataRequestHandler::OrderedRequests::~OrderedRequests() = default;

void DataRequestHandler::OrderedRequests::Add(Request* request) {
  DCHECK(Find(request->handle) == requests_.end());

  const auto offset = request->offset;
  const auto it = std::find_if(
      requests_.begin(), requests_.end(),
      [offset](const Request& request) { return request.offset <= offset; });
  requests_.insert(it, std::move(*request));

  if (VLOG_IS_ON(7)) {
    VLOG(7) << " PROPMEDIA(GPU) : " << __FUNCTION__ << " Ongoing requests:";
    for (const auto& r : requests_) {
      VLOG(7) << " PROPMEDIA(GPU) : " << __FUNCTION__ << "  " << r.handle
              << ": " << r.offset;
    }
  }
}

DataRequestHandler::Request DataRequestHandler::OrderedRequests::Remove(
    id handle) {
  Request request;
  const auto it = Find(handle);
  if (it != requests_.end()) {
    request = std::move(*it);
    requests_.erase(it);
  }
  return request;
}

DataRequestHandler::Request DataRequestHandler::OrderedRequests::Pop() {
  DCHECK(!requests_.empty());
  Request request = std::move(requests_.back());
  requests_.pop_back();
  return request;
}

DataRequestHandler::OrderedRequests::Container::iterator
DataRequestHandler::OrderedRequests::Find(id handle) {
  return std::find_if(
      requests_.begin(), requests_.end(),
      [handle](const Request& request) { return request.handle == handle; });
}

DataRequestHandler::DataRequestHandler() = default;

DataRequestHandler::~DataRequestHandler() {
  // Must either be stopped at this point or destructed before
  // InitSourceReader() call.
  DCHECK(!source_reader_);
  VLOG(5) << " PROPMEDIA(GPU) : " << __FUNCTION__;
}

void DataRequestHandler::InitSourceReader(
    ipc_data_source::Reader source_reader) {
  // This can only be called once.
  DCHECK(!source_reader_);

  source_reader_ = std::move(source_reader);
}

void DataRequestHandler::InitCallbacks(RespondWithDataCB respond_with_data_cb,
                                       FinishLoadingCB finish_loading_cb) {
  // This can only be called once.
  DCHECK(!respond_with_data_cb.is_null());
  DCHECK(!finish_loading_cb.is_null());
  DCHECK(respond_with_data_cb_.is_null());
  DCHECK(finish_loading_cb_.is_null());
  respond_with_data_cb_ = std::move(respond_with_data_cb);
  finish_loading_cb_ = std::move(finish_loading_cb);
}

void DataRequestHandler::HandleDataRequest(id request_handle,
                                           int64_t offset,
                                           int64_t length) {
  DCHECK(request_handle != nil);
  DCHECK(CanHandleRequests());
  Request request(request_handle, offset, length);
  if (waiting_for_reply_) {
    VLOG(5) << " PROPMEDIA(GPU) : " << __FUNCTION__
            << " request_handle=" << request.handle
            << " queue_size=" << pending_requests_.size() + 1
            << " length=" << request.length << " offset=" << request.offset;
    pending_requests_.Add(&request);
  } else {
    DispatchRead(&request);
  }
}

void DataRequestHandler::CancelDataRequest(id request_handle) {
  VLOG(5) << " PROPMEDIA(GPU) : " << __FUNCTION__
          << " request_handle=" << request_handle;

  if (active_request_.handle == request_handle) {
    DCHECK(waiting_for_reply_);
    FinishLoading(&active_request_, Status::kCancelled);
  } else {
    Request request = pending_requests_.Remove(request_handle);
    FinishLoading(&request, Status::kCancelled);
  }
}

void DataRequestHandler::AbortAllDataRequests() {
  // We only clear waiting_for_reply_ when we get the reply.
  if (active_request_.handle) {
    DCHECK(waiting_for_reply_);
    FinishLoading(&active_request_, Status::kAborted);
  }
  while (!pending_requests_.is_empty()) {
    Request request = pending_requests_.Pop();
    FinishLoading(&request, Status::kAborted);
  }
}

void DataRequestHandler::Stop() {
  VLOG(5) << " PROPMEDIA(GPU) : " << __FUNCTION__
          << " stopped=" << source_reader_.is_null()
          << " waiting_for_reply_=" << waiting_for_reply_
          << " pending_requests_.size()=" << pending_requests_.size();

  if (!source_reader_)
    return;

  // Clear source_reader_ before we execute any callbacks so they can query for
  // IsStopped().
  source_reader_.Reset();
  AbortAllDataRequests();

  // Clear callbacks to break any reference cycles.
  respond_with_data_cb_.Reset();
  finish_loading_cb_.Reset();
}

bool DataRequestHandler::IsStopped() const {
  return source_reader_.is_null();
}

void DataRequestHandler::Suspend() {
  VLOG(5) << " PROPMEDIA(GPU) : " << __FUNCTION__
          << " stopped=" << source_reader_.is_null()
          << " waiting_for_reply_=" << waiting_for_reply_
          << " pending_requests_.size()=" << pending_requests_.size()
          << " suspended_=" << suspended_;

  if (suspended_)
    return;
  suspended_= true;
  AbortAllDataRequests();
}

void DataRequestHandler::Resume() {
  VLOG(5) << " PROPMEDIA(GPU) : " << __FUNCTION__
          << " stopped=" << source_reader_.is_null()
          << " waiting_for_reply_=" << waiting_for_reply_
          << " pending_requests_.size()=" << pending_requests_.size()
          << " suspended_=" << suspended_;

  suspended_ = false;
}

bool DataRequestHandler::CanHandleRequests() {
  return source_reader_ && !suspended_;
}

bool DataRequestHandler::IsHandlingDataRequests() const {
  // We return true to indicate that we called the finish callback for all
  // requests. waiting_for_reply_ can still be true at this point if the active
  // request was aborted or cancelled.
  return active_request_.handle || pending_requests_.size() != 0;
}

void DataRequestHandler::DispatchRead(Request* request) {
  DCHECK(!waiting_for_reply_);
  waiting_for_reply_ = true;
  active_request_ = std::move(*request);

  int chunk_size = kMaxReadChunkSize;
  if (active_request_.length > 0 &&
      active_request_.length < static_cast<int64_t>(kMaxReadChunkSize)) {
    chunk_size = static_cast<int>(active_request_.length);
  }

  VLOG(5) << " PROPMEDIA(GPU) : " << __FUNCTION__
          << " request_handle=" << active_request_.handle
          << " length=" << active_request_.length
          << " chunk_size=" << chunk_size
          << " continues_read=" << (stream_position_ == active_request_.offset)
          << " offset=" << active_request_.offset;

  stream_position_ = active_request_.offset;
  TRACE_EVENT0("IPC_MEDIA", __FUNCTION__);
  source_reader_.Run(active_request_.offset, chunk_size,
                   base::BindOnce(&DataRequestHandler::DidReadNextChunk, this));
}

void DataRequestHandler::DidReadNextChunk(int read_size, const uint8_t* data) {
  DCHECK(waiting_for_reply_);

  VLOG(5) << " PROPMEDIA(GPU) : " << __FUNCTION__
          << " request_handle=" << active_request_.handle
          << " read_size=" << read_size
          << " offset=" << active_request_.offset;

  do {
    Request request = std::move(active_request_);
    waiting_for_reply_ = false;

    if (!request.handle) {
      // Canceled or aborted.
      break;
    }

    if (read_size < 0) {
      FinishLoading(&request, Status::kError);
      Stop();
      break;
    }
    if (read_size == 0) {
      VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__ << " eos=true";
      FinishLoading(&request, Status::kEOS);
      break;
    }
    DCHECK(request.length < 0 || read_size <= request.length);
    stream_position_ += read_size;

    respond_with_data_cb_.Run(request.handle, data, read_size);

    // We're done if we read all the data that was requested
    if (request.length > 0) {
      if (request.length == read_size) {
        FinishLoading(&request, Status::kSuccess);
        break;
      }
      request.length -= read_size;
    }
    request.offset += read_size;
    pending_requests_.Add(&request);
  } while (false);

  if (!pending_requests_.is_empty()) {
    DCHECK(source_reader_);
    Request request = pending_requests_.Pop();
    DispatchRead(&request);
  }
}

void DataRequestHandler::FinishLoading(Request* request, Status status) {
  if (request->handle) {
    VLOG(5) << " PROPMEDIA(GPU) : " << __FUNCTION__
            << " request_handle=" << request->handle
            << " status=" << static_cast<int>(status);
    finish_loading_cb_.Run(request->handle, status);
    request->handle.reset();
  }
}

}  // namespace media
