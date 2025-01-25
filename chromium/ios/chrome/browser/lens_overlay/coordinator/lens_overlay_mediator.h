// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LENS_OVERLAY_COORDINATOR_LENS_OVERLAY_MEDIATOR_H_
#define IOS_CHROME_BROWSER_LENS_OVERLAY_COORDINATOR_LENS_OVERLAY_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/lens_overlay/coordinator/lens_omnibox_client_delegate.h"
#import "ios/chrome/browser/lens_overlay/ui/lens_overlay_result_consumer.h"
#import "ios/chrome/browser/lens_overlay/ui/lens_toolbar_mutator.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_focus_delegate.h"
#import "ios/public/provider/chrome/browser/lens/lens_overlay_api.h"

@protocol ChromeLensOverlay;
@protocol LensOverlayCommands;
@protocol LensToolbarConsumer;
@class OmniboxCoordinator;
class TemplateURLService;
namespace web {
class WebState;
}  // namespace web

/// Main mediator for Lens Overlay.
/// Manages data flow between Selection, Omnibox and Results.
@interface LensOverlayMediator : NSObject <ChromeLensOverlayDelegate,
                                           LensOmniboxClientDelegate,
                                           LensToolbarMutator,
                                           OmniboxFocusDelegate>

@property(nonatomic, weak) id<LensOverlayResultConsumer> resultConsumer;

// Handler for the Lens Overlay commands;
@property(nonatomic, weak) id<LensOverlayCommands> commandsHandler;

/// Coordinator to interact with the omnibox.
@property(nonatomic, weak) OmniboxCoordinator* omniboxCoordinator;

/// Lens toolbar consumer.
@property(nonatomic, weak) id<LensToolbarConsumer> toolbarConsumer;

/// Lens backend handler.
@property(nonatomic, weak) id<ChromeLensOverlay> lensHandler;

/// Active`webState` observed by this mediator.
@property(nonatomic, assign) web::WebState* webState;

/// TemplateURLService to observe default search engine change.
@property(nonatomic, assign) TemplateURLService* templateURLService;

/// Releases managed objects.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_LENS_OVERLAY_COORDINATOR_LENS_OVERLAY_MEDIATOR_H_
