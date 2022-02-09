// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA.

#include "platform_media/gpu/decoders/mac/data_source_loader.h"

#include "base/logging.h"
#include "platform_media/gpu/decoders/mac/data_request_handler.h"

@implementation DataSourceLoader

- (id)initWithHandler:(const scoped_refptr<media::DataRequestHandler>&)handler {
  self = [super init];
  if (self) {
    handler_ = handler;
  }
  return self;
}

- (BOOL)resourceLoader:(AVAssetResourceLoader*)resourceLoader
    shouldWaitForLoadingOfRequestedResource:
        (AVAssetResourceLoadingRequest*)loadingRequest {
  media::DispatchQueueRunnerProxy::ScopedRunner scoped_runner;

  handler_->Load(loadingRequest);
  return YES;
}

- (void)resourceLoader:(AVAssetResourceLoader*)resourceLoader
    didCancelLoadingRequest:(AVAssetResourceLoadingRequest*)loadingRequest {
  media::DispatchQueueRunnerProxy::ScopedRunner scoped_runner;

  // Sometimes, the resource loader cancels requests that have finished (and
  // then we may have pruned them.)
  if ([loadingRequest isFinished]) {
    VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__  << " already_finished"
            << " request=" << static_cast<void*>(loadingRequest);
  } else {
    handler_->CancelRequest(loadingRequest);
  }
}

@end
