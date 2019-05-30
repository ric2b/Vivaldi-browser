// Copyright 2018 Vivaldi Thecnologies. All rights reserved.

#ifndef VIVALDI_UI_FULLSCREEN_MENUBAR_TRACKER_MAC_H_
#define VIVALDI_UI_FULLSCREEN_MENUBAR_TRACKER_MAC_H_

#import <Cocoa/Cocoa.h>

class VivaldiNativeAppWindowViewsMac;

@interface VivaldiFullscreenMenubarTracker : NSObject

- (instancetype)initWithVivaldiNativeAppWindow:
    (VivaldiNativeAppWindowViewsMac*)owner;
- (void)dispatchFullscreenMenubarChangedEvent:(bool)shown;

@end

#endif  // VIVALDI_UI_FULLSCREEN_MENUBAR_TRACKER_MAC_H_