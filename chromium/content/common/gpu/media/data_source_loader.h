// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA.

#ifndef CONTENT_COMMON_GPU_MEDIA_DATA_SOURCE_LOADER_H_
#define CONTENT_COMMON_GPU_MEDIA_DATA_SOURCE_LOADER_H_

#import <AVFoundation/AVFoundation.h>

#include "base/containers/scoped_ptr_hash_map.h"
#include "base/mac/scoped_nsobject.h"
#include "base/memory/ref_counted.h"
#include "media/base/mac/avfoundation_glue.h"

namespace content {
class DataRequestHandler;
class IPCDataSource;
}  // namespace content

@interface DataSourceLoader : NSObject<CrAVAssetResourceLoaderDelegate> {
 @private
  base::scoped_nsobject<NSString> contentType_;
  content::IPCDataSource* dataSource_;
  dispatch_queue_t queue_;
  scoped_refptr<content::DataRequestHandler> handler_;
  base::scoped_nsobject<CrAVAssetResourceLoadingRequest> lastRequest_;
}
- (id)initWithDataSource:(content::IPCDataSource*)dataSource
            withMIMEType:(NSString*)mimeType;

- (void)stop;

- (BOOL)resourceLoader:(CrAVAssetResourceLoader*)resourceLoader
    shouldWaitForLoadingOfRequestedResource:
        (CrAVAssetResourceLoadingRequest*)loadingRequest;
- (void)resourceLoader:(CrAVAssetResourceLoader*)resourceLoader
    didCancelLoadingRequest:(CrAVAssetResourceLoadingRequest*)loadingRequest;
- (dispatch_queue_t)dispatchQueue;

@end

#endif  // CONTENT_COMMON_GPU_MEDIA_DATA_SOURCE_LOADER_H_
