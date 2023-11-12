// Copyright 2022 Vivaldi Technologies. All rights reserved.

#ifndef IOS_CHROME_BROWSER_UI_VIVALDI_SPEED_DIAL_BASE_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_VIVALDI_SPEED_DIAL_BASE_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/ntp/vivaldi_speed_dial_item.h"

@protocol VivaldiSpeedDialBaseControllerDelegate
- (void)didTapSpeedDialWithItem:(VivaldiSpeedDialItem*)item
                captureSnapshot:(BOOL)captureSnapshot;
@end

// The controller that contains the speed dial folder menu and the child pages.
@interface VivaldiSpeedDialBaseController : UIViewController

// INITIALIZER
- (instancetype)initWithBrowser:(Browser*)browser;

// DELEGATE
@property(nonatomic, weak) id<VivaldiSpeedDialBaseControllerDelegate> delegate;

// SETTERS
/// Remove all pushed view controllers from the navigation stack and show the root view controller
- (void)popPushedViewControllers;

@end

#endif  // IOS_CHROME_BROWSER_UI_VIVALDI_SPEED_DIAL_BASE_CONTROLLER_H_
