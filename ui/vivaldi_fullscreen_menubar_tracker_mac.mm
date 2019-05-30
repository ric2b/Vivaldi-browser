// Copyright 2018 Vivaldi Technologies. All rights reserved.

#import "ui/vivaldi_fullscreen_menubar_tracker_mac.h"

#include <Carbon/Carbon.h>

#include "base/stl_util.h"
#include "ui/vivaldi_native_app_window_views_mac.h"
#include "ui/vivaldi_browser_window.h"

namespace vivaldi {

OSStatus MenuBarChangedHandler(EventHandlerCallRef handler,
                              EventRef event,
                              void* context) {
  VivaldiFullscreenMenubarTracker* self =
      static_cast<VivaldiFullscreenMenubarTracker*>(context);

  if (GetEventKind(event) == kEventMenuBarShown) {
    [self dispatchFullscreenMenubarChangedEvent:true];
  } else if (GetEventKind(event) == kEventMenuBarHidden) {
    [self dispatchFullscreenMenubarChangedEvent:false];
  }

  return CallNextEventHandler(handler, event);
}

}  // end namespace

@interface VivaldiFullscreenMenubarTracker () {
  VivaldiNativeAppWindowViewsMac* owner_;        // weak

  // A Carbon event handler that tracks the state of the menubar.
  EventHandlerRef menubarTrackingHandler_;
}

@end

@implementation VivaldiFullscreenMenubarTracker

- (instancetype)initWithVivaldiNativeAppWindow:
    (VivaldiNativeAppWindowViewsMac*)owner {
  if ((self = [super init])) {
    owner_ = owner;
    // Install the Carbon event handler for the menubar show, hide and
    // undocumented reveal event.
    EventTypeSpec eventSpecs[2];

    eventSpecs[0].eventClass = kEventClassMenu;
    eventSpecs[0].eventKind = kEventMenuBarShown;

    eventSpecs[1].eventClass = kEventClassMenu;
    eventSpecs[1].eventKind = kEventMenuBarHidden;

    InstallApplicationEventHandler(NewEventHandlerUPP(&vivaldi::MenuBarChangedHandler),
                                   base::size(eventSpecs), eventSpecs, self,
                                   &menubarTrackingHandler_);
  }
  return self;
}

- (void)dispatchFullscreenMenubarChangedEvent:(bool)shown {
  if (![owner_->window()->GetNativeWindow().GetNativeNSWindow()
              isOnActiveSpace]) {
    return;
  }
  if (![self isMouseOnScreen]) {
    return;
  }

  owner_->DispatchFullscreenMenubarChangedEvent(shown);
}

- (void)dealloc {
  RemoveEventHandler(menubarTrackingHandler_);

  [super dealloc];
}

- (BOOL)isMouseOnScreen {
  return NSMouseInRect(
      [NSEvent mouseLocation],
      [owner_->window()->GetNativeWindow().GetNativeNSWindow() screen].frame,
      false);
}

@end
