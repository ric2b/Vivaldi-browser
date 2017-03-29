// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA.

#include "content/common/gpu/media/data_source_loader.h"

#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "content/common/gpu/media/data_request_handler.h"
#include "content/common/gpu/media/ipc_data_source.h"

namespace content {
namespace {

void DataAvailable(id request_handle, uint8_t* data, int size) {
  DVLOG(3) << "Responding to " << request_handle << " with " << size
           << " bytes of data";

  auto loading_request =
      reinterpret_cast<CrAVAssetResourceLoadingRequest*>(request_handle);
  NSData* ns_data =
      [NSData dataWithBytesNoCopy:data length:size freeWhenDone:NO];
  [[loading_request dataRequest] respondWithData:ns_data];
}

void LoadingFinished(id request_handle, DataRequestHandler::Status status) {
  DVLOG(3) << "Signalling loading finished of " << request_handle << ": "
           << status;

  auto loading_request =
      reinterpret_cast<CrAVAssetResourceLoadingRequest*>(request_handle);
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
}  // namespace content

namespace {

std::string ToHumanReadableString(
    CrAVAssetResourceLoadingRequest* loadingRequest) {
  CrAVAssetResourceLoadingDataRequest* dataRequest =
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

- (id)initWithDataSource:(content::IPCDataSource*)dataSource
            withMIMEType:(NSString*)mimeType {
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
    DVLOG(1) << "Resolved '" << base::SysNSStringToUTF8(mimeType) << "' as '"
             << base::SysNSStringToUTF8(contentType_) << "'";

    dataSource_ = dataSource;

    queue_ = dispatch_queue_create("com.operasoftware.DataSourceLoader",
                                   DISPATCH_QUEUE_SERIAL);

    handler_ = new content::DataRequestHandler(
        dataSource_, base::Bind(&content::DataAvailable),
        base::Bind(&content::LoadingFinished), queue_);
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
  DVLOG(1) << "~DataSourceLoader()";
  DCHECK([self isStopped]);

  dispatch_release(queue_);
  [super dealloc];
}

- (void)fillContentInformation:
            (CrAVAssetResourceLoadingContentInformationRequest*)
        contentInformationRequest {
  DCHECK(queue_ == dispatch_get_current_queue());

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

- (void)handleRequest:(CrAVAssetResourceLoadingRequest*)loadingRequest {
  DCHECK(queue_ == dispatch_get_current_queue());

  if ([loadingRequest contentInformationRequest])
    [self fillContentInformation:[loadingRequest contentInformationRequest]];

  if (![loadingRequest dataRequest]) {
    DVLOG(1) << "No data request.";
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
      DVLOG(1) << "Request rejected due to streaming restrictions";
      [loadingRequest
          finishLoadingWithError:[NSError errorWithDomain:NSPOSIXErrorDomain
                                                     code:EIO
                                                 userInfo:nil]];
      return;
    }
  }

  DVLOG(1) << "New request from resource loader: "
           << ToHumanReadableString(loadingRequest);
  lastRequest_.reset([loadingRequest retain]);
  handler_->HandleDataRequest(
      loadingRequest,
      content::ResourceLoadingDataRequest([loadingRequest dataRequest]));
}

- (BOOL)resourceLoader:(CrAVAssetResourceLoader*)resourceLoader
    shouldWaitForLoadingOfRequestedResource:
        (CrAVAssetResourceLoadingRequest*)loadingRequest {
  DCHECK(queue_ == dispatch_get_current_queue());

  if ([self isStopped])
    return NO;

  DVLOG(1) << [[[[loadingRequest request] URL] absoluteString] UTF8String];
  [[[loadingRequest request] allHTTPHeaderFields]
      enumerateKeysAndObjectsUsingBlock:^(id key, id value, BOOL* stop) {
          DVLOG(1) << [base::mac::ObjCCast<NSString>(key) UTF8String] << ": "
                   << [base::mac::ObjCCast<NSString>(value) UTF8String];
      }];

  [self handleRequest:loadingRequest];

  return YES;
}

- (void)resourceLoader:(CrAVAssetResourceLoader*)resourceLoader
    didCancelLoadingRequest:(CrAVAssetResourceLoadingRequest*)loadingRequest {
  DCHECK(queue_ == dispatch_get_current_queue());

  if ([self isStopped])
    return;

  // Sometimes, the resource loader cancels requests that have finished (and
  // then we may have pruned them.)
  if (![loadingRequest isFinished]) {
    DVLOG(1) << "Resource loader is canceling request: "
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
