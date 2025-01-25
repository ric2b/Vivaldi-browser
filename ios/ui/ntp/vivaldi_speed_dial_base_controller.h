// Copyright 2022 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_NTP_VIVALDI_SPEED_DIAL_BASE_CONTROLLER_H_
#define IOS_UI_NTP_VIVALDI_SPEED_DIAL_BASE_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/ui/ntp/vivaldi_speed_dial_home_consumer.h"
#import "ios/ui/ntp/vivaldi_speed_dial_view_controller_delegate.h"

namespace bookmarks {
  class BookmarkModel;
}

// The controller that contains the speed dial folder menu and the child pages.
@interface VivaldiSpeedDialBaseController : UIViewController
    <SpeedDialHomeConsumer>

// DELEGATE
@property (nonatomic, weak) id<SpeedDialViewControllerDelegate> delegate;

// INITIALIZER
- (instancetype)initWithBrowser:(Browser*)browser
                  bookmarkModel:(bookmarks::BookmarkModel*)bookmarkModel;

- (void)invalidate;

@end

#endif  // IOS_UI_NTP_VIVALDI_SPEED_DIAL_BASE_CONTROLLER_H_
