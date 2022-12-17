// Copyright 2022 Vivaldi Technologies. All rights reserved.

#ifndef IOS_CHROME_BROWSER_UI_VIVALDI_UIVIEWCONTROLLER_HELPER_H_
#define IOS_CHROME_BROWSER_UI_VIVALDI_UIVIEWCONTROLLER_HELPER_H_

#import <UIKit/UIKit.h>

@interface UIViewController(Vivaldi)

#pragma mark:- GETTERS
/// Returns whether the device orientation is valid. When device is flat or orientation is invalid UI layout is
/// not changed.
- (BOOL)hasValidOrientation;
/// Returns whether the device is in portrait mode or vice-versa
- (BOOL)isDevicePortrait;

@end

#endif  // IOS_CHROME_BROWSER_UI_VIVALDI_UIVIEWCONTROLLER_HELPER_H_
