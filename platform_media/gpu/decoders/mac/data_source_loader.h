// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA.

#ifndef PLATFORM_MEDIA_GPU_DECODERS_MAC_DATA_SOURCE_LOADER_H_
#define PLATFORM_MEDIA_GPU_DECODERS_MAC_DATA_SOURCE_LOADER_H_

#include "platform_media/common/feature_toggles.h"

#include "base/memory/scoped_refptr.h"

#import <AVFoundation/AVFoundation.h>

namespace media {
class DataRequestHandler;
}  // namesoace media

/**
 * "The AVAssetResourceLoaderDelegate protocol defines a method that lets your
 * code handle resource loading requests coming from an AVURLAsset object."
 * https://developer.apple.com/documentation/avfoundation/avassetresourceloaderdelegate
 * http://blog.jaredsinclair.com/post/149892449150/implementing-avassetresourceloaderdelegate-a
 **/

@interface DataSourceLoader : NSObject<AVAssetResourceLoaderDelegate> {
 @private
  scoped_refptr<media::DataRequestHandler> handler_;
}

- (id)initWithHandler:(const scoped_refptr<media::DataRequestHandler>&)handler;

// Asks the delegate if it wants to load the requested resource.
- (BOOL)resourceLoader:(AVAssetResourceLoader*)resourceLoader
    shouldWaitForLoadingOfRequestedResource:
        (AVAssetResourceLoadingRequest*)loadingRequest;

  // Invoked to inform the delegate that a prior loading request has been cancelled
- (void)resourceLoader:(AVAssetResourceLoader*)resourceLoader
    didCancelLoadingRequest:(AVAssetResourceLoadingRequest*)loadingRequest;

@end

#endif  // PLATFORM_MEDIA_GPU_DECODERS_MAC_DATA_SOURCE_LOADER_H_
