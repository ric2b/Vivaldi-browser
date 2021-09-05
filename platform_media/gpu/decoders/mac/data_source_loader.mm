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

/*
@interface AVURLAsset (MavericksSDK)
@property(nonatomic, readonly) AVAssetResourceLoader* resourceLoader;
@end
*/

namespace media {

namespace {

bool ShouldReadAll(AVAssetResourceLoadingDataRequest* dataRequest) {
  if (@available(macOS 10.11, *)) {
    return [dataRequest requestsAllDataToEndOfResource];
  }
  return false;
}

std::string ToHumanReadableString(
    AVAssetResourceLoadingRequest* loadingRequest) {
  AVAssetResourceLoadingDataRequest* dataRequest =
      [loadingRequest dataRequest];
  bool read_all = ShouldReadAll(dataRequest);
  int64_t requested_length =
      read_all ? -1LL
               : base::checked_cast<int64_t>([dataRequest requestedLength]);
  int64_t requested_offset = [dataRequest requestedOffset];
  int64_t read_length = [dataRequest currentOffset] - requested_offset;
  int64_t remaining = read_all ? -1LL : requested_length - read_length;
  return base::StringPrintf(" request(offset=%lld length=%lld "
                            "read_length=%lld remaining=%lld)",
                            requested_offset, requested_length, read_length,
                            remaining);
}

void DataAvailable(id request_handle, const uint8_t* data, int size) {
  auto loading_request =
      reinterpret_cast<AVAssetResourceLoadingRequest*>(request_handle);
  VLOG(2) << " PROPMEDIA(GPU) : " << __FUNCTION__
          << ToHumanReadableString(loading_request) << " size=" << size;

  NSData* ns_data = [NSData dataWithBytesNoCopy:const_cast<uint8_t*>(data)
                                         length:size
                                   freeWhenDone:NO];
  [[loading_request dataRequest] respondWithData:ns_data];
}

void LoadingFinished(id request_handle, DataRequestHandler::Status status) {
  auto loading_request =
      reinterpret_cast<AVAssetResourceLoadingRequest*>(request_handle);
  VLOG(2) << " PROPMEDIA(GPU) : " << __FUNCTION__
          << ToHumanReadableString(loading_request)
          << " status=" << static_cast<int>(status);

  DCHECK(![loading_request isFinished]);

  switch (status) {
    case DataRequestHandler::Status::kSuccess:
    case DataRequestHandler::Status::kEOS:
      [loading_request finishLoading];
      break;

    case DataRequestHandler::Status::kError:
    case DataRequestHandler::Status::kAborted:
      [loading_request
          finishLoadingWithError:[NSError errorWithDomain:NSPOSIXErrorDomain
                                                     code:EIO
                                                 userInfo:nil]];
      break;

    case DataRequestHandler::Status::kCancelled:
      // Nothing to do.
      break;
  }
}

}  // namespace
}  // namespace media

@implementation DataSourceLoader

- (id)initWithDataSource:(ipc_data_source::Reader)source_reader
          withSourceInfo:(ipc_data_source::Info)source_info
       withDispatchQueue:(dispatch_queue_t)dispatch_queue {
  VLOG(2) << " PROPMEDIA(GPU) : " << __FUNCTION__;

  self = [super init];
  if (!self)
    return self;

  dispatch_queue_.reset(dispatch_queue, base::scoped_policy::RETAIN);

  contentType_.reset(
      base::mac::CFToNSCast(UTTypeCreatePreferredIdentifierForTag(
          kUTTagClassMIMEType,
          base::mac::NSToCFCast(base::SysUTF8ToNSString(source_info.mime_type)),
          nullptr)));
  // From Apple documentation, "If no result is found, this function creates
  // a dynamic type beginning with the dyn prefix".
  if ([contentType_ hasPrefix:@"dyn"]) {
    // If MIME type was not recognized, fall back to something that we know
    // works well for most of the media that DataSourceLoader needs to work
    // with.
    contentType_.reset(AVFileTypeMPEG4);
  }

  isStreaming_ = source_info.is_streaming;
  dataSize_ = source_info.size;

  handler_ = base::MakeRefCounted<media::DataRequestHandler>();
  handler_->InitSourceReader(std::move(source_reader));
  handler_->InitCallbacks(base::Bind(&media::DataAvailable),
                          base::Bind(&media::LoadingFinished));
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

  // setDelegate does not retain the delegate so this does not create a
  // reference loop.
  [[url_asset_ resourceLoader] setDelegate:self queue:dispatch_queue_];

  VLOG(2) << " PROPMEDIA(GPU) : " << __FUNCTION__
          << " url=" << [[url absoluteString] UTF8String]
          << " mime_type=" << source_info.mime_type
          << " mac_content_type=" << base::SysNSStringToUTF8(contentType_)
          << " data_size=" << dataSize_ << " is_streaming=" << isStreaming_;

  return self;
}

- (void)dealloc {
  VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
          << " ~DataSourceLoader()";
  [super dealloc];
}

- (AVAsset*)asset {
  return url_asset_.get();
}

- (void) stop {
  VLOG(2) << " PROPMEDIA(GPU) : " << __FUNCTION__;
  DCHECK(dispatch_queue_get_label(dispatch_queue_) ==
         dispatch_queue_get_label(DISPATCH_CURRENT_QUEUE_LABEL));
  handler_->Stop();
  [[url_asset_ resourceLoader] setDelegate:nil queue:nil];
}

- (void) suspend {
  VLOG(2) << " PROPMEDIA(GPU) : " << __FUNCTION__;
  DCHECK(dispatch_queue_get_label(dispatch_queue_) ==
         dispatch_queue_get_label(DISPATCH_CURRENT_QUEUE_LABEL));
  handler_->Suspend();
}

- (void) resume {
  VLOG(2) << " PROPMEDIA(GPU) : " << __FUNCTION__;
  DCHECK(dispatch_queue_get_label(dispatch_queue_) ==
         dispatch_queue_get_label(DISPATCH_CURRENT_QUEUE_LABEL));
  handler_->Resume();
}

- (void)fillContentInformation:
            (AVAssetResourceLoadingContentInformationRequest*)
        contentInformationRequest {
  VLOG(2) << " PRVOPMEDIA(GPU) : " << __FUNCTION__;

  DCHECK(dispatch_queue_get_label(dispatch_queue_) ==
         dispatch_queue_get_label(DISPATCH_CURRENT_QUEUE_LABEL));

  [contentInformationRequest setByteRangeAccessSupported:YES];
  [contentInformationRequest setContentType:contentType_];

  int64_t length = dataSize_;
  if (length < 0) {
    // DataSource size is unknown, but we have to set _some_ content length or
    // the resource loader will not ask for more data.  On the one hand, we
    // want the greatest value possible here.  On the other hand, the resource
    // loader also bails out if the value is too big.
    length = 4ll * 1024 * 1024 * 1024 * 1024;
  }

  [contentInformationRequest setContentLength:length];
}

- (BOOL)resourceLoader:(AVAssetResourceLoader*)resourceLoader
    shouldWaitForLoadingOfRequestedResource:
        (AVAssetResourceLoadingRequest*)loadingRequest {
  DCHECK(dispatch_queue_get_label(dispatch_queue_) ==
         dispatch_queue_get_label(DISPATCH_CURRENT_QUEUE_LABEL));

  if ([loadingRequest contentInformationRequest]) {
    // We are asked to fill meta-information, ignore any dataRequest, see
    // https://jaredsinclair.com/2016/09/03/implementing-avassetresourceload.html
    // and
    // https://developer.apple.com/documentation/avfoundation/avassetresourceloadingcontentinformationrequest?language=objc
    [self fillContentInformation:[loadingRequest contentInformationRequest]];
    [loadingRequest finishLoading];
    return YES;
  }

  AVAssetResourceLoadingDataRequest* dataRequest = [loadingRequest dataRequest];
  if (!dataRequest) {
    VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
            << " No data request.";
    return NO;
  }
  VLOG(2) << " PRVOPMEDIA(GPU) : " << __FUNCTION__
          << media::ToHumanReadableString(loadingRequest);

  if (!handler_->CanHandleRequests()) {
    VLOG(1) << " PRVOPMEDIA(GPU) : " << __FUNCTION__
            << " can_handle=false";
    return NO;
  }

  if (VLOG_IS_ON(3)) {
    [[[loadingRequest request] allHTTPHeaderFields]
        enumerateKeysAndObjectsUsingBlock:^(id key, id value, BOOL* stop) {
          VLOG(3) << " PROPMEDIA(GPU) : " << __FUNCTION__
                  << [base::mac::ObjCCast<NSString>(key) UTF8String] << ": "
                  << [base::mac::ObjCCast<NSString>(value) UTF8String];
        }];
  }

  int64_t offset = [dataRequest requestedOffset];
  if (isStreaming_) {
    // We are streaming, so no concurrent requests and no reading in future.
    bool can_require = false;
    if (!handler_->IsHandlingDataRequests()) {
      int64_t last_offset = handler_->stream_position();
      if (last_offset < 0) {
        // This is the first read.
        can_require = true;
      } else {
        // Can proceed if asking for data right after just read or in past.
        can_require = (offset <= last_offset);
      }
    }
    if (!can_require) {
      VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
              << " Request rejected due to streaming restrictions";
      return NO;
    }
  }

  int64_t length = -1;
  if (!media::ShouldReadAll(dataRequest)) {
    // Casting to NSUInteger is required in case 32 bit builds and
    // data_request.length value exceeds INT32_MAX which is returned as a
    // negative value.
    length = static_cast<NSUInteger>([dataRequest requestedLength]);
    if (length == 0) {
      VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
              << " dataRequest with zero length";
      [loadingRequest finishLoading];
      return YES;
    }
  }
  handler_->HandleDataRequest(loadingRequest, offset, length);

  return YES;
}

- (void)resourceLoader:(AVAssetResourceLoader*)resourceLoader
    didCancelLoadingRequest:(AVAssetResourceLoadingRequest*)loadingRequest {
  DCHECK(dispatch_queue_get_label(dispatch_queue_) ==
         dispatch_queue_get_label(DISPATCH_CURRENT_QUEUE_LABEL));

  // Sometimes, the resource loader cancels requests that have finished (and
  // then we may have pruned them.)
  if ([loadingRequest isFinished]) {
    VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__  << " already_finished"
            << media::ToHumanReadableString(loadingRequest);
  } else {
    VLOG(2) << " PROPMEDIA(GPU) : " << __FUNCTION__
            << media::ToHumanReadableString(loadingRequest);
    handler_->CancelDataRequest(loadingRequest);
  }
}

@end
