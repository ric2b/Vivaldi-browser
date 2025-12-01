// Copyright 2024-25 Vivaldi Technologies. All rights reserved.

#import "ios/ui/settings/pagezoom/dialog/vivaldi_pagezoom_dialog_mediator.h"

#import "base/memory/raw_ptr.h"
#import "base/scoped_observation.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/text_zoom_commands.h"
#import "ios/chrome/browser/text_zoom/ui_bundled/text_zoom_consumer.h"
#import "ios/chrome/browser/web/model/font_size/font_size_tab_helper.h"
#import "ios/public/provider/chrome/browser/text_zoom/text_zoom_api.h"
#import "ios/ui/helpers/vivaldi_global_helpers.h"
#import "ios/ui/settings/pagezoom/vivaldi_pagezoom_settings_prefs.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/public/favicon/favicon_status.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "ios/web/public/web_state.h"
#import "ui/gfx/image/image.h"

@interface VivaldiPageZoomDialogMediator ()
    <WebStateListObserving, CRWWebStateObserver>
@end

@implementation VivaldiPageZoomDialogMediator {
  // The WebStateList observed by this mediator and the observer bridge.
  raw_ptr<WebStateList> _webStateList;
  std::unique_ptr<WebStateListObserver> _webStateListObserver;

  // The active WebState of the WebStateList and the observer bridge. It
  // is observed to detect navigation and to close the UI when they happen.
  raw_ptr<web::WebState> _activeWebState;
  std::unique_ptr<web::WebStateObserver> _activeWebStateObserver;

  // The handler for any TextZoom commands.
  id<TextZoomCommands> _commandHandler;
}

- (instancetype)initWithWebStateList:(WebStateList*)webStateList
                      commandHandler:(id<TextZoomCommands>)commandHandler {
  DCHECK(webStateList);
  DCHECK(commandHandler);
  if (([super init])) {
    _webStateList = webStateList;
    _commandHandler = commandHandler;
    _activeWebState = _webStateList->GetActiveWebState();

    // Create and register the observers.
    _webStateListObserver = std::make_unique<WebStateListObserverBridge>(self);
    _webStateList->AddObserver(_webStateListObserver.get());

    _activeWebStateObserver =
        std::make_unique<web::WebStateObserverBridge>(self);
    if (_activeWebState) {
      _activeWebState->AddObserver(_activeWebStateObserver.get());
    }
  }
  return self;
}

- (void)dealloc {
  DCHECK(!_activeWebState);
  DCHECK(!_webStateList);
}

- (void)disconnect {
  if (_activeWebState) {
    _activeWebState->RemoveObserver(_activeWebStateObserver.get());
    _activeWebStateObserver.reset();
    _activeWebState = nullptr;
  }

  if (_webStateList) {
    _webStateList->RemoveObserver(_webStateListObserver.get());
    _webStateListObserver = nullptr;
    _webStateList = nullptr;
  }
}

#pragma mark - Accessors

- (void)setConsumer:(id<VivaldiPageZoomDialogConsumer>)consumer {
  _consumer = consumer;
  [self updateConsumerState];
}

#pragma mark - WebStateListObserver

- (void)didChangeWebStateList:(WebStateList*)webStateList
                       change:(const WebStateListChange&)change
                       status:(const WebStateListStatus&)status {
  DCHECK_EQ(_webStateList, webStateList);
  if (status.active_web_state_change()) {
    [self setActiveWebState:status.new_active_web_state];
    [_commandHandler closeTextZoom];
  }
}

#pragma mark - PageZoomHandler

- (void)zoomIn {
  if (_activeWebState) {
    FontSizeTabHelper* fontSizeTabHelper =
      FontSizeTabHelper::FromWebState(_activeWebState);
    if (fontSizeTabHelper) {
      fontSizeTabHelper->UserZoom(ZOOM_IN);
    }
  }
  [self updateConsumerState];
}

- (void)zoomOut {
  if (_activeWebState) {
    FontSizeTabHelper* fontSizeTabHelper =
      FontSizeTabHelper::FromWebState(_activeWebState);
    if (fontSizeTabHelper) {
      fontSizeTabHelper->UserZoom(ZOOM_OUT);
    }
  }
  [self updateConsumerState];
}

- (void)resetZoom {
  if (_activeWebState) {
    FontSizeTabHelper* fontSizeTabHelper =
      FontSizeTabHelper::FromWebState(_activeWebState);
    if (fontSizeTabHelper) {
      fontSizeTabHelper->UserZoom(ZOOM_RESET);
    }
  }
  [self updateConsumerState];
}

- (void)refreshState {
  [self updateConsumerState];
}

#pragma mark - CRWWebStateObserver

- (void)webStateDestroyed:(web::WebState*)webState {
  [self setActiveWebState:nullptr];
  [_commandHandler closeTextZoom];
}

- (void)webState:(web::WebState*)webState
    didFinishNavigation:(web::NavigationContext*)navigation {
  [_commandHandler closeTextZoom];
}

#pragma mark - Private methods

- (void)setActiveWebState:(web::WebState*)webState {
  if (_activeWebState) {
    _activeWebState->RemoveObserver(_activeWebStateObserver.get());
    _activeWebState = nullptr;
  }

  if (webState) {
    _activeWebState = webState;
    _activeWebState->AddObserver(_activeWebStateObserver.get());
  }
}

- (void) updateConsumerState {
  if (!_activeWebState || !_prefService) {
    return;
  }
  FontSizeTabHelper* activeFontSizeTabHelper =
    FontSizeTabHelper::FromWebState(_activeWebState);
  if (!activeFontSizeTabHelper) {
    return;
  }

  // Up UI States for zoom label plus, miuns and reset buttons
  int level = activeFontSizeTabHelper->GetFontZoomSize();
  [_consumer setCurrentZoomLevel:level];
  [_consumer setZoomInEnabled:activeFontSizeTabHelper->CanUserZoomIn()];
  [_consumer setZoomOutEnabled:activeFontSizeTabHelper->CanUserZoomOut()];
  [_consumer setResetZoomEnabled:activeFontSizeTabHelper->CanUserResetZoom()];

  // Updating favicon and URL
  GURL url = _activeWebState->GetLastCommittedURL();
  NSString* stringUrl = base::SysUTF8ToNSString(url.host().c_str());
  [_consumer setCurrentHostURL:
    [VivaldiGlobalHelpers hostOfURLString: stringUrl]];
  const web::FaviconStatus& favicon_status =
    _activeWebState->GetFaviconStatus();
  if (_activeWebState->GetFaviconStatus().valid) {
    gfx::Image gfxImage = favicon_status.image;
    if (!gfxImage.IsEmpty()) {
      [_consumer setCurrentHostFavicon: gfxImage.ToUIImage()];
    }
  }
}

@end
