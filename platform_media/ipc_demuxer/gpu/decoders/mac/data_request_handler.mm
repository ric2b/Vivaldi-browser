// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA.

#include "platform_media/ipc_demuxer/gpu/decoders/mac/data_request_handler.h"

#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/trace_event/trace_event.h"

#include "platform_media/ipc_demuxer/gpu/data_source/ipc_data_source.h"
#import "platform_media/ipc_demuxer/gpu/decoders/mac/data_source_loader.h"

namespace media {

DispatchQueueRunnerProxy* DispatchQueueRunnerProxy::g_proxy = nullptr;

namespace {

constexpr int kBadRequestErrorCode = 400;

bool ShouldReadAll(AVAssetResourceLoadingDataRequest* data_request) {
  if (@available(macOS 10.11, *)) {
    return [data_request requestsAllDataToEndOfResource];
  }
  return false;
}

int64_t GetLengthToReadAndOffset(
    AVAssetResourceLoadingDataRequest* data_request,
    int64_t* offset) {
  *offset = data_request.currentOffset;
  if (ShouldReadAll(data_request))
    return -1;
  int64_t already_read = *offset - data_request.requestedOffset;
  return data_request.requestedLength - already_read;
}

}  // namespace

DataRequestHandler::DataRequestHandler() = default;

DataRequestHandler::~DataRequestHandler() {
  // Must either be stopped at this point or destructed before
  // InitSourceReader() call.
  DCHECK(!can_read_);
  VLOG(5) << " PROPMEDIA(GPU) : " << __FUNCTION__;
}

void DataRequestHandler::Init(ipc_data_source::Info source_info,
                              dispatch_queue_t ipc_queue) {
  // This can only be called once.
  DCHECK(!can_read_);
  can_read_ = true;
  source_buffer_ = std::move(source_info.buffer);
  is_streaming_ = source_info.is_streaming;
  data_size_ = source_info.size;

  if (!ipc_queue) {
    if (DispatchQueueRunnerProxy::enabled()) {
      ipc_queue = DispatchQueueRunnerProxy::instance()->GetQueue();
    }
    if (!ipc_queue) {
      ipc_queue = dispatch_get_main_queue();
    }
  }
  ipc_queue_.reset(ipc_queue, base::scoped_policy::RETAIN);

  content_type_.reset(
      base::mac::CFToNSCast(UTTypeCreatePreferredIdentifierForTag(
          kUTTagClassMIMEType,
          base::mac::NSToCFCast(base::SysUTF8ToNSString(source_info.mime_type)),
          nullptr)));
  // From Apple documentation, "If no result is found, this function creates
  // a dynamic type beginning with the dyn prefix".
  if ([content_type_ hasPrefix:@"dyn"]) {
    // If MIME type was not recognized, fall back to something that we know
    // works well for most of the media that DataSourceLoader needs to work
    // with.
    content_type_.reset(AVFileTypeMPEG4);
  }

  // Use a "custom" URL scheme to force AVURLAsset to ask our instance of
  // AVAssetResourceLoaderDelegate for data.  This way, we make sure all data
  // is fetched through Chrome's network stack rather than by AVURLAsset
  // directly.
  // AVPlayer does not play some links (without extension, with query or
  // containing some characters like ';'). To avoid all future problems with
  // invalid URL, file name set in AVURLAsset is constant.
  //
  // This technique is also described here:
  // http://vombat.tumblr.com/post/86294492874/caching-audio-streamed-using-avplayer
  NSURL* url = [NSURL URLWithString:@"opop://media_file.mp4"];
  url_asset_.reset([AVURLAsset assetWithURL:url], base::scoped_policy::RETAIN);

  VLOG(2) << " PROPMEDIA(GPU) : " << __FUNCTION__
          << " url=" << [[url absoluteString] UTF8String]
          << " mime_type=" << source_info.mime_type
          << " mac_content_type=" << base::SysNSStringToUTF8(content_type_)
          << " data_size=" << data_size_ << " is_streaming=" << is_streaming_;

  data_source_loader_.reset([[DataSourceLoader alloc] initWithHandler:this]);

  // setDelegate does not retain the delegate so this does not create a
  // reference loop.
  [[url_asset_ resourceLoader] setDelegate:data_source_loader_
                                     queue:GetIPCQueue()];
}

dispatch_queue_t DataRequestHandler::GetIPCQueue() {
  return ipc_queue_.get();
}

AVURLAsset* DataRequestHandler::GetAsset() {
  return url_asset_.get();
}

void DataRequestHandler::Load(AVAssetResourceLoadingRequest* request) {
  // The request is released in CloseRequest.
  [request retain];

  if (!can_read_) {
    CloseRequest(request, Status::kError);
    return;
  }

  if ([request contentInformationRequest]) {
    // We are asked to fill meta-information, ignore any dataRequest, see
    // https://jaredsinclair.com/2016/09/03/implementing-avassetresourceload.html
    // and
    // https://developer.apple.com/documentation/avfoundation/avassetresourceloadingcontentinformationrequest?language=objc
    FillContentInformation([request contentInformationRequest]);
    CloseRequest(request, Status::kSuccess);
    return;
  }

  AVAssetResourceLoadingDataRequest* data_request = [request dataRequest];
  if (!data_request) {
    VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__ << " No data request.";
    CloseRequest(request, Status::kBadRequest);
    return;
  }

  if (data_size_ == 0) {
    // This is a sandbox initialization and we already reported 0 size to
    // contentInformationRequest and should not be called again.
    VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__ << " empty read attempt";
    CloseRequest(request, Status::kBadRequest);
    return;
  }

  int64_t offset = [data_request requestedOffset];
  int64_t length = -1;
  if (!ShouldReadAll(data_request)) {
    length = [data_request requestedLength];
    if (length == 0) {
      VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
              << " data_request with zero length";
      CloseRequest(request, Status::kSuccess);
      return;
    }
  }

  DCHECK(offset >= 0);
  DCHECK(request != nil);
  if (!CanHandleRequests()) {
    VLOG(1) << " PRVOPMEDIA(GPU) : " << __FUNCTION__ << " can_handle=false";
    CloseRequest(request, Status::kBadRequest);
    return;
  }

  if (is_streaming_) {
    if (!source_buffer_) {
      VLOG(1)
          << " PROPMEDIA(GPU) : " << __FUNCTION__
          << " Attempt to perform streaming request while waiting for a reply";
      CancelRequest(request);
      return;
    }
    // No concurrent reads nor jumps into future when streaming.
    if (IsHandlingDataRequests()) {
      VLOG(1)
          << " PROPMEDIA(GPU) : " << __FUNCTION__
          << " Cannot perform a conscurrent read due to streaming restrictions";
      CloseRequest(request, Status::kBadRequest);
      return;
    }
    if (!before_first_read_ && offset > source_buffer_.GetLastReadEnd()) {
      VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
              << " Cannot skip into future due to streaming restrictions";
      CloseRequest(request, Status::kBadRequest);
      return;
    }
  }

  if (!source_buffer_) {
    // We either have an active request that waits for an reply or the active
    // request was cancelled and we have not received a reply for it. Wait until
    // the reply arrives.
    VLOG(5) << " PROPMEDIA(GPU) : " << __FUNCTION__
            << " request=" << static_cast<void*>(request.request)
            << " queue_size=" << pending_requests_.size() + 1
            << " length=" << length
            << " offset=" << request.dataRequest.requestedOffset;
    pending_requests_.push_back(request);
  } else {
    DCHECK(!active_request_);
    DispatchRead(request, offset, length);
  }
}

void DataRequestHandler::FillContentInformation(
    AVAssetResourceLoadingContentInformationRequest* information_request) {
  VLOG(2) << " PRVOPMEDIA(GPU) : " << __FUNCTION__;

  [information_request setByteRangeAccessSupported:YES];
  [information_request setContentType:content_type_];

  int64_t length = data_size_;
  if (length < 0) {
    // DataSource size is unknown, but we have to set _some_ content length or
    // the resource loader will not ask for more data.  On the one hand, we
    // want the greatest value possible here.  On the other hand, the resource
    // loader also bails out if the value is too big.
    length = 4ll * 1024 * 1024 * 1024 * 1024;
  }

  [information_request setContentLength:length];
}

void DataRequestHandler::CancelRequest(AVAssetResourceLoadingRequest* request) {
  VLOG(5) << " PROPMEDIA(GPU) : " << __FUNCTION__
          << " request=" << static_cast<void*>(request);
  DCHECK(request);
  if (!can_read_)
    return;

  if (active_request_ == request) {
    active_request_ = nil;
    CloseRequest(request, Status::kCancelled);
  } else {
    auto i =
        std::find(pending_requests_.begin(), pending_requests_.end(), request);
    if (i != pending_requests_.end()) {
      pending_requests_.erase(i);
      CloseRequest(request, Status::kCancelled);
    }
  }
}

void DataRequestHandler::AbortAllDataRequests() {
  if (active_request_) {
    CloseRequest(active_request_, Status::kAborted);
    active_request_ = nil;
  }
  while (!pending_requests_.empty()) {
    AVAssetResourceLoadingRequest* request = pending_requests_.back();
    pending_requests_.pop_back();
    CloseRequest(request, Status::kAborted);
  }
}

void DataRequestHandler::Stop() {
  DCHECK_EQ(dispatch_queue_get_label(ipc_queue_),
            dispatch_queue_get_label(DISPATCH_CURRENT_QUEUE_LABEL));
  VLOG(5) << " PROPMEDIA(GPU) : " << __FUNCTION__ << " can_read=" << can_read_
          << " waiting_for_reply=" << !source_buffer_
          << " pending_requests_.size()=" << pending_requests_.size();

  if (!can_read_)
    return;

  // Clear the flag before we execute any callbacks so they can query for
  // IsStopped().
  can_read_ = false;

  [[url_asset_ resourceLoader] setDelegate:nil queue:nil];

  // Break a reference cycle.
  data_source_loader_.reset();

  AbortAllDataRequests();
}

bool DataRequestHandler::IsStopped() const {
  return !can_read_;
}

void DataRequestHandler::Suspend() {
  DCHECK(dispatch_queue_get_label(ipc_queue_) ==
         dispatch_queue_get_label(DISPATCH_CURRENT_QUEUE_LABEL));
  VLOG(5) << " PROPMEDIA(GPU) : " << __FUNCTION__ << " can_read=" << can_read_
          << " waiting_for_reply=" << !source_buffer_
          << " pending_requests_.size()=" << pending_requests_.size()
          << " suspended_=" << suspended_;

  if (suspended_)
    return;
  suspended_ = true;
  AbortAllDataRequests();
}

void DataRequestHandler::Resume() {
  DCHECK(dispatch_queue_get_label(ipc_queue_) ==
         dispatch_queue_get_label(DISPATCH_CURRENT_QUEUE_LABEL));
  VLOG(5) << " PROPMEDIA(GPU) : " << __FUNCTION__ << " can_read=" << can_read_
          << " waiting_for_reply=" << !source_buffer_
          << " pending_requests_.size()=" << pending_requests_.size()
          << " suspended_=" << suspended_;

  suspended_ = false;
}

bool DataRequestHandler::IsSuspended() const {
  return suspended_;
}

bool DataRequestHandler::CanHandleRequests() {
  return can_read_ && !suspended_;
}

bool DataRequestHandler::IsHandlingDataRequests() const {
  // We return true to indicate that we called the finish callback for all
  // requests.
  return active_request_ || pending_requests_.size() != 0;
}

void DataRequestHandler::DispatchRead(AVAssetResourceLoadingRequest* request,
                                      int64_t offset,
                                      int64_t length) {
  DCHECK(can_read_);
  DCHECK(source_buffer_);
  DCHECK(request);
  DCHECK(!active_request_);
  active_request_ = request;

  int chunk_size = source_buffer_.GetCapacity();
  if (length > 0 && length < chunk_size) {
    chunk_size = static_cast<int>(length);
  }

  if (offset == source_buffer_.GetReadPosition() &&
      chunk_size == source_buffer_.GetRequestedSize() &&
      source_buffer_.GetReadSize() > 0) {
    // This a read request for data that arrived after the previous request was
    // cancelled. Do not read them again.
    VLOG(4) << " PROPMEDIA(GPU) : " << __FUNCTION__ << " using_cached_data";
    DidReadNextChunk(std::move(source_buffer_));
    return;
  }

  VLOG(5) << " PROPMEDIA(GPU) : " << __FUNCTION__
          << " request=" << static_cast<void*>(active_request_)
          << " length=" << length << " chunk_size=" << chunk_size
          << " continues_read=" << (source_buffer_.GetLastReadEnd() == offset)
          << " offset=" << offset;

  TRACE_EVENT0("IPC_MEDIA", __FUNCTION__);
  before_first_read_ = false;
  source_buffer_.SetReadRange(offset, chunk_size);

  ipc_data_source::Buffer::Read(
      std::move(source_buffer_),
      base::BindOnce(&DataRequestHandler::DidReadNextChunk, this));
}

void DataRequestHandler::DidReadNextChunk(
    ipc_data_source::Buffer source_buffer) {
  DCHECK(source_buffer);
  DCHECK(!source_buffer_);
  source_buffer_ = std::move(source_buffer);

  VLOG(5) << " PROPMEDIA(GPU) : " << __FUNCTION__
          << " request=" << static_cast<void*>(active_request_.request)
          << " read_size=" << source_buffer_.GetReadSize()
          << " offset=" << source_buffer_.GetReadPosition();

  do {
    AVAssetResourceLoadingRequest* request = active_request_;
    if (!request) {
      // Canceled or aborted.
      break;
    }
    active_request_ = nil;

    int read_size = source_buffer_.GetReadSize();
    if (read_size < 0) {
      CloseRequest(request, Status::kError);
      Stop();
      break;
    }
    if (read_size == 0) {
      VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__ << " eos=true";
      CloseRequest(request, Status::kSuccess);
      break;
    }

    AVAssetResourceLoadingDataRequest* data_request = request.dataRequest;

    // Do not use [NSData dataWithBytesNoCopy] and always make a copy of data in
    // the IPC buffer. At least on MacOS 10.15 the data can be accessed after
    // respondWithData returns.
    NSData* ns_data =
        [NSData dataWithBytes:const_cast<uint8_t*>(source_buffer_.GetReadData())
                       length:read_size];
    [data_request respondWithData:ns_data];

    int64_t offset;
    int64_t length = GetLengthToReadAndOffset(data_request, &offset);
    if (length == 0) {
      // we are done
      CloseRequest(request, Status::kSuccess);
      break;
    }
    if (pending_requests_.empty()) {
      // This is a typical case. So skip adding to the queue and continue to
      // read the request.
      DispatchRead(request, offset, length);
      return;
    }

    pending_requests_.push_back(request);

  } while (false);

  if (!pending_requests_.empty()) {
    // Dispatch the request with a minimal current offset.
    DCHECK(can_read_);
    auto i = std::min_element(pending_requests_.begin(),
                              pending_requests_.end(), [](auto r1, auto r2) {
                                return r1.dataRequest.currentOffset <
                                       r2.dataRequest.currentOffset;
                              });
    AVAssetResourceLoadingRequest* request = *i;
    pending_requests_.erase(i);

    int64_t offset;
    int64_t length = GetLengthToReadAndOffset(request.dataRequest, &offset);
    VLOG(4) << " PROPMEDIA(GPU) : " << __FUNCTION__
            << " removed_from_queue min_offset=" << offset
            << " queue_length=" << pending_requests_.size();
    DispatchRead(request, offset, length);
  }
}

void DataRequestHandler::CloseRequest(AVAssetResourceLoadingRequest* request,
                                      Status status) {
  VLOG(5) << " PROPMEDIA(GPU) : " << __FUNCTION__
          << " request=" << static_cast<void*>(request)
          << " status=" << static_cast<int>(status);
  DCHECK(![request isFinished]);

  switch (status) {
    case DataRequestHandler::Status::kSuccess:
      [request finishLoading];
      break;

    case DataRequestHandler::Status::kBadRequest:
      [request
          finishLoadingWithError:[NSError errorWithDomain:NSURLErrorDomain
                                                     code:kBadRequestErrorCode
                                                 userInfo:nil]];
      break;

    case DataRequestHandler::Status::kError:
    case DataRequestHandler::Status::kAborted: {
      NSInteger code = (status == Status::kError) ? EIO : EINTR;
      [request
          finishLoadingWithError:[NSError errorWithDomain:NSPOSIXErrorDomain
                                                     code:code
                                                 userInfo:nil]];
      break;
    }

    case DataRequestHandler::Status::kCancelled:
      // Nothing to do.
      break;
  }

  // Compensate for retain in Load().
  [request release];
}

}  // namespace media
