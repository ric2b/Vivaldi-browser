// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA.

#include "platform_media/gpu/decoders/mac/data_source_loader.h"

#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "platform_media/gpu/decoders/mac/data_request_handler.h"
#include "platform_media/gpu/data_source/ipc_data_source.h"

namespace media {
namespace {

void DataAvailable(id request_handle, uint8_t* data, int size) {
  VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
          << " Responding to " << request_handle << " with " << size
          << " bytes of data";

  auto loading_request =
      reinterpret_cast<AVAssetResourceLoadingRequest*>(request_handle);
  NSData* ns_data =
      [NSData dataWithBytesNoCopy:data length:size freeWhenDone:NO];
  [[loading_request dataRequest] respondWithData:ns_data];
}

void LoadingFinished(id request_handle, DataRequestHandler::Status status) {
  VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
          << " Signalling loading finished of " << request_handle << ": "
          << status;

  auto loading_request =
      reinterpret_cast<AVAssetResourceLoadingRequest*>(request_handle);
  DCHECK(![loading_request isFinished]);

  switch (status) {
    case DataRequestHandler::SUCCESS:
      [loading_request finishLoading];
      break;

    case DataRequestHandler::ERROR:
      [loading_request
          finishLoadingWithError:[NSError errorWithDomain:NSPOSIXErrorDomain
                                                     code:EIO
                                                 userInfo:nil]];
      break;

    case DataRequestHandler::CANCELED:
      // Nothing to do.
      break;

    default:
      NOTREACHED();
  }
}

}  // namespace
}  // namespace media

namespace {

std::string ToHumanReadableString(
    AVAssetResourceLoadingRequest* loadingRequest) {
  AVAssetResourceLoadingDataRequest* dataRequest =
      [loadingRequest dataRequest];
  return base::StringPrintf(
      "requested offset: %lld, requested length: %lld, current offset: %lld",
      [dataRequest requestedOffset],
      base::checked_cast<int64_t>([dataRequest requestedLength]),
      [dataRequest currentOffset]);
}

}  // namespace


@interface DataSourceLoader()

- (int64_t)lastOffset;

@end

@implementation DataSourceLoader

- (id)initWithDataSource:(media::DataSource*)dataSource
            withMIMEType:(NSString*)mimeType {

  VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__;

  if ((self = [super init])) {
    contentType_.reset(
        base::mac::CFToNSCast(UTTypeCreatePreferredIdentifierForTag(
            kUTTagClassMIMEType, base::mac::NSToCFCast(mimeType), nullptr)));
    // From Apple documentation, "If no result is found, this function creates
    // a dynamic type beginning with the dyn prefix".
    if ([contentType_ hasPrefix:@"dyn"]) {
      // If MIME type was not recognized, fall back to something that we know
      // works well for most of the media that DataSourceLoader needs to work
      // with.
      contentType_.reset(@"public.mpeg-4");
    }
    VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
            << " Resolved '" << base::SysNSStringToUTF8(mimeType) << "' as '"
            << base::SysNSStringToUTF8(contentType_) << "'";

    dataSource_ = dataSource;

    queue_ = dispatch_queue_create("com.operasoftware.DataSourceLoader",
                                   DISPATCH_QUEUE_SERIAL);

    handler_ = new media::DataRequestHandler(
        dataSource_, base::Bind(&media::DataAvailable),
        base::Bind(&media::LoadingFinished), queue_);
  }
  return self;
}

- (void)stop {
  DCHECK(queue_ != dispatch_get_current_queue());

  // Stop the data source while not on the dispatch queue to unblock any
  // pending DataRequestHandlers.
  dataSource_->Stop();

  dispatch_sync(queue_, ^{
      // We will not accept further data requests.
      dataSource_ = nullptr;

      handler_->AbortAllDataRequests();
  });
}

- (bool)isStopped {
  return dataSource_ == nullptr;
}

- (void)dealloc {
  VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
          << " ~DataSourceLoader()";
  DCHECK([self isStopped]);

  dispatch_release(queue_);
  [super dealloc];
}

- (void)fillContentInformation:
            (AVAssetResourceLoadingContentInformationRequest*)
        contentInformationRequest {
  DCHECK(queue_ == dispatch_get_current_queue());

  VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
          << " content type " << contentType_;

  [contentInformationRequest setByteRangeAccessSupported:YES];
  [contentInformationRequest setContentType:contentType_];

  int64_t length = -1;
  if (!dataSource_->GetSize(&length)) {
    // DataSource size is unknown, but we have to set _some_ content length or
    // the resource loader will not ask for more data.  On the one hand, we
    // want the greatest value possible here.  On the other hand, the resource
    // loader also bails out if the value is too big.
    length = 4ll * 1024 * 1024 * 1024 * 1024;
  }

  [contentInformationRequest setContentLength:length];
}

- (void)handleRequest:(AVAssetResourceLoadingRequest*)loadingRequest {
  DCHECK(queue_ == dispatch_get_current_queue());

  if ([loadingRequest contentInformationRequest])
    [self fillContentInformation:[loadingRequest contentInformationRequest]];

  if (![loadingRequest dataRequest]) {
    VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
            << " No data request.";
    [loadingRequest finishLoading];
    return;
  }

  if (dataSource_->IsStreaming()) {
    // We are streaming, so no concurrent requests and no reading in future.
    // The only exception is the special request for bytes 0-1 the resource
    // loader always makes.
    const bool requesting_future_data =
        [[loadingRequest dataRequest] requestedOffset] > [self lastOffset] + 1;
    if (handler_->IsHandlingDataRequests() ||
        ([self lastOffset] > 2 && requesting_future_data)) {
      VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
              << " Request rejected due to streaming restrictions";
      [loadingRequest
          finishLoadingWithError:[NSError errorWithDomain:NSPOSIXErrorDomain
                                                     code:EIO
                                                 userInfo:nil]];
      return;
    }
  }

  VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
          << " New request from resource loader: "
          << ToHumanReadableString(loadingRequest);
  lastRequest_.reset([loadingRequest retain]);
  handler_->HandleDataRequest(
      loadingRequest,
      media::ResourceLoadingDataRequest([loadingRequest dataRequest]));
}

- (BOOL)resourceLoader:(AVAssetResourceLoader*)resourceLoader
    shouldWaitForLoadingOfRequestedResource:
        (AVAssetResourceLoadingRequest*)loadingRequest {
  DCHECK(queue_ == dispatch_get_current_queue());

  if ([self isStopped])
    return NO;

  VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
          << [[[[loadingRequest request] URL] absoluteString] UTF8String];

  [[[loadingRequest request] allHTTPHeaderFields]
      enumerateKeysAndObjectsUsingBlock:^(id key, id value, BOOL* stop) {
          VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
                  << [base::mac::ObjCCast<NSString>(key) UTF8String] << ": "
                  << [base::mac::ObjCCast<NSString>(value) UTF8String];
      }];

  [self handleRequest:loadingRequest];

  return YES;
}

- (void)resourceLoader:(AVAssetResourceLoader*)resourceLoader
    didCancelLoadingRequest:(AVAssetResourceLoadingRequest*)loadingRequest {
  DCHECK(queue_ == dispatch_get_current_queue());

  VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__;

  if ([self isStopped])
    return;

  // Sometimes, the resource loader cancels requests that have finished (and
  // then we may have pruned them.)
  if (![loadingRequest isFinished]) {
    VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
            << " Resource loader is canceling request: "
            << ToHumanReadableString(loadingRequest);
    handler_->CancelDataRequest(loadingRequest);
  }
}

- (dispatch_queue_t)dispatchQueue {
  DCHECK(queue_);
  return queue_;
}

- (int64_t)lastOffset {
  return lastRequest_ ? [[lastRequest_ dataRequest] currentOffset] - 1 : -1;
}

@end
