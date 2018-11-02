// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA.

#ifndef CONTENT_COMMON_GPU_MEDIA_DATA_SOURCE_LOADER_H_
#define CONTENT_COMMON_GPU_MEDIA_DATA_SOURCE_LOADER_H_

#import <AVFoundation/AVFoundation.h>

#include "base/mac/scoped_nsobject.h"
#include "base/memory/ref_counted.h"
#include "media/base/data_source.h"

namespace content {
class DataRequestHandler;
class IPCDataSource;
}  // namespace content

@interface DataSourceLoader : NSObject<AVAssetResourceLoaderDelegate> {
 @private
  base::scoped_nsobject<NSString> contentType_;
  media::DataSource* dataSource_;
  dispatch_queue_t queue_;
  scoped_refptr<content::DataRequestHandler> handler_;
  base::scoped_nsobject<AVAssetResourceLoadingRequest> lastRequest_;
}
- (id)initWithDataSource:(media::DataSource*)dataSource
            withMIMEType:(NSString*)mimeType;

- (void)stop;

- (BOOL)resourceLoader:(AVAssetResourceLoader*)resourceLoader
    shouldWaitForLoadingOfRequestedResource:
        (AVAssetResourceLoadingRequest*)loadingRequest;
- (void)resourceLoader:(AVAssetResourceLoader*)resourceLoader
    didCancelLoadingRequest:(AVAssetResourceLoadingRequest*)loadingRequest;
- (dispatch_queue_t)dispatchQueue;

@end

#endif  // CONTENT_COMMON_GPU_MEDIA_DATA_SOURCE_LOADER_H_
