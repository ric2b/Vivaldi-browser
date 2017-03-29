// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA.

#include "content/common/gpu/media/data_request_handler.h"

#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "content/common/gpu/media/ipc_data_source.h"

namespace content {

struct DataRequestHandler::Request {
  explicit Request(id handle, const ResourceLoadingDataRequest& data_request);
  ~Request();

  base::scoped_nsobject<id> handle;
  ResourceLoadingDataRequest data_request;
};

DataRequestHandler::Request::Request(
    id handle,
    const ResourceLoadingDataRequest& data_request)
    : handle([handle retain]), data_request(data_request) {
}

DataRequestHandler::Request::~Request() = default;

// A container for Requests uniquely identified by handles, but ordered by
// requested offset.
class DataRequestHandler::OrderedRequests {
 public:
  void Add(const Request& request);
  void Remove(id handle);
  void RemoveAll();
  const Request& Top() const;
  void Pop();

  bool is_empty() const { return requests_.empty(); }

 private:
  using Container = std::vector<Request>;

  Container::iterator Find(id handle);

  Container requests_;
};

void DataRequestHandler::OrderedRequests::Add(const Request& request) {
  DCHECK(Find(request.handle) == requests_.end());

  const auto offset = request.data_request.offset;
  const auto it = std::find_if(requests_.begin(), requests_.end(),
                               [offset](const Request& request) {
    return request.data_request.offset <= offset;
  });
  requests_.insert(it, request);

  DVLOG(7) << "Ongoing requests:";
  for (const auto& r : requests_)
    DVLOG(7) << "  " << r.handle << ": " << r.data_request.offset;
}

void DataRequestHandler::OrderedRequests::Remove(id handle) {
  const auto it = Find(handle);
  DCHECK(it != requests_.end());
  requests_.erase(it);
}

void DataRequestHandler::OrderedRequests::RemoveAll() {
  requests_.clear();
}

const DataRequestHandler::Request& DataRequestHandler::OrderedRequests::Top()
    const {
  DCHECK(!requests_.empty());
  return requests_.back();
}

void DataRequestHandler::OrderedRequests::Pop() {
  DCHECK(!requests_.empty());
  requests_.pop_back();
}

DataRequestHandler::OrderedRequests::Container::iterator
DataRequestHandler::OrderedRequests::Find(id handle) {
  return std::find_if(
      requests_.begin(), requests_.end(),
      [handle](const Request& request) { return request.handle == handle; });
}

DataRequestHandler::DataRequestHandler(
    IPCDataSource* data_source,
    const RespondWithDataCB& respond_with_data_cb,
    const FinishLoadingCB& finish_loading_cb,
    dispatch_queue_t queue)
    : data_source_(data_source),
      queue_(queue),
      requests_(new OrderedRequests),
      last_size_read_(IPCDataSource::kReadError),
      respond_with_data_cb_(respond_with_data_cb),
      finish_loading_cb_(finish_loading_cb),
      read_complete_(false, false) {
  DCHECK(data_source_ != NULL);
  DCHECK(!respond_with_data_cb_.is_null());
  DCHECK(!finish_loading_cb_.is_null());
}

DataRequestHandler::~DataRequestHandler() {
  DVLOG(5) << __FUNCTION__;
}

void DataRequestHandler::HandleDataRequest(
    id request_handle,
    const ResourceLoadingDataRequest& data_request) {
  DVLOG(5) << "New data request " << request_handle << ": "
           << data_request.length << " bytes @" << data_request.offset << "...";
  DCHECK(queue_ == dispatch_get_current_queue());

  requests_->Add(Request(request_handle, data_request));

  DispatchBlockingRead();
}

void DataRequestHandler::CancelDataRequest(id request_handle) {
  DVLOG(5) << __FUNCTION__;
  DCHECK(queue_ == dispatch_get_current_queue());

  requests_->Remove(request_handle);

  FinishLoading(request_handle, CANCELED);
}

void DataRequestHandler::AbortAllDataRequests() {
  DVLOG(5) << __FUNCTION__;
  DCHECK(queue_ == dispatch_get_current_queue());

  while (!requests_->is_empty()) {
    FinishLoading(requests_->Top().handle, ERROR);
    requests_->Pop();
  }
}

bool DataRequestHandler::IsHandlingDataRequests() const {
  DCHECK(queue_ == dispatch_get_current_queue());
  return !requests_->is_empty();
}

void DataRequestHandler::DispatchBlockingRead() {
  DCHECK(queue_ == dispatch_get_current_queue());

  // Let the block below capture |this| by copying the scoped_refptr.  This
  // creates an additional reference that can then be safely used within the
  // block even if all other references are gone.
  scoped_refptr<DataRequestHandler> handler(this);

  dispatch_async(queue_, ^{
    if (!handler->requests_->is_empty())
      handler->ReadNextChunk();
    else
      DVLOG(1) << "All requests have been canceled";
  });
}

void DataRequestHandler::ReadNextChunk() {
  DCHECK(queue_ == dispatch_get_current_queue());

  const ResourceLoadingDataRequest& data_request =
      requests_->Top().data_request;

  DCHECK_GT(data_request.length, 0);
  DVLOG(5) << "Blocking to read @" << data_request.offset;

  // We're performing a blocking read to enforce proper serialization of reads
  // on the dispatch queue.  AVAssetResourceLoader sometimes makes overlapping
  // read requests, but an IPCDataSource cannot handle overlapping reads.

  TRACE_EVENT0("IPC_MEDIA", __FUNCTION__);
  data_source_->Read(data_request.offset,
                     chunk_size(data_request),
                     buffer_,
                     base::Bind(&DataRequestHandler::DidReadNextChunk, this));
  read_complete_.Wait();

  DVLOG(5) << "...chunk complete";
  ProcessChunk();
}

void DataRequestHandler::DidReadNextChunk(int size) {
  // We don't control which thread or dispatch queue this is called on.  All
  // we're doing here must be thread-safe.

  last_size_read_ = size;
  read_complete_.Signal();
}

void DataRequestHandler::ProcessChunk() {
  DCHECK(queue_ == dispatch_get_current_queue());

  Request request = requests_->Top();
  requests_->Pop();

  DVLOG(5) << "Processing chunk: " << last_size_read_ << " @"
           << request.data_request.offset;

  if (last_size_read_ <= 0) {
    DVLOG_IF(1, last_size_read_ == 0) << "DataSource reports EOS";
    FinishLoading(request.handle, ERROR);
    AbortAllDataRequests();
    return;
  }

  respond_with_data_cb_.Run(request.handle, buffer_, last_size_read_);

  DCHECK_LE(last_size_read_, request.data_request.length);
  // We're done if we read all the data that was requested or the source has
  // less data than was requested.
  if (request.data_request.length == last_size_read_ ||
      last_size_read_ < chunk_size(request.data_request)) {
    FinishLoading(request.handle, SUCCESS);
  } else {
    request.data_request.length -= last_size_read_;
    request.data_request.offset += last_size_read_;
    requests_->Add(request);
  }

  if (!requests_->is_empty())
    DispatchBlockingRead();
}

void DataRequestHandler::FinishLoading(id request_handle, Status status) {
  DVLOG(5) << "Finishing " << request_handle << ": " << status;
  DCHECK(queue_ == dispatch_get_current_queue());

  finish_loading_cb_.Run(request_handle, status);
}

}  // namespace content
