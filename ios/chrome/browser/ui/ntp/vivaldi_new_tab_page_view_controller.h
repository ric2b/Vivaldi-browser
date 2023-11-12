// Copyright 2022 Vivaldi Technologies. All rights reserved.

#ifndef IOS_CHROME_BROWSER_UI_VIVALDI_NEW_TAB_PAGE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_VIVALDI_NEW_TAB_PAGE_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/ntp/vivaldi_speed_dial_item.h"

@protocol VivaldiNewTabPageViewControllerDelegate
- (void)didTapSpeedDial:(VivaldiSpeedDialItem*)item
        captureSnapshot:(BOOL)captureSnapshot;
@end
@protocol PopupMenuCommands;

// The controller holds the fake search bar and the view controller that
// contains the speed dial folder menu and the child pages.
@interface VivaldiNewTabPageViewController
    : UIViewController

// INITIALIZER
- (instancetype)initWithBrowser:(Browser*)browser;

// DELEGATE
@property(nonatomic,weak) id<VivaldiNewTabPageViewControllerDelegate> delegate;
// Popup menu commands handler for the ViewController.
@property(nonatomic, weak) id<PopupMenuCommands> popupMenuCommandsHandler;

@end

#endif  // IOS_CHROME_BROWSER_UI_VIVALDI_NEW_TAB_PAGE_VIEW_CONTROLLER_H_
