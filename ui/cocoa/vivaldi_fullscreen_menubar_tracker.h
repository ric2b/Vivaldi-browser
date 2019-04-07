// Copyright 2018 Vivaldi Thecnologies. All rights reserved.

#ifndef VIVALDI_UI_COCOA_FULLSCREEN_MENUBAR_TRACKER_H_
#define VIVALDI_UI_COCOA_FULLSCREEN_MENUBAR_TRACKER_H_

#import <Cocoa/Cocoa.h>

class VivaldiNativeAppWindowCocoa;

@interface VivaldiFullscreenMenubarTracker : NSObject

- (instancetype)initWithVivaldiNativeAppWindow:
    (VivaldiNativeAppWindowCocoa*)owner;
- (void)dispatchFullscreenMenubarChangedEvent:(bool)shown;

@end

#endif  // VIVALDI_UI_COCOA_FULLSCREEN_MENUBAR_TRACKER_H_