// Copyright 2022 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_NTP_VIVALDI_SPEED_DIAL_HOME_MEDIATOR_H_
#define IOS_UI_NTP_VIVALDI_SPEED_DIAL_HOME_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "components/bookmarks/browser/bookmark_model.h"
#import "ios/ui/ntp/vivaldi_speed_dial_home_consumer.h"
#import "ios/ui/ntp/vivaldi_speed_dial_item.h"
#import "ios/ui/ntp/vivaldi_speed_dial_sorting_mode.h"
#import "ios/ui/ntp/vivaldi_speed_dial_view_controller_delegate.h"

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace bookmarks {
class BookmarkModel;
}

class ProfileIOS;

// VivaldiSpeedDialHomeMediator manages model interactions for the
// VivaldiNewTabPageViewController.
@interface VivaldiSpeedDialHomeMediator :
    NSObject<SpeedDialViewControllerDelegate>

@property(nonatomic, weak) id<SpeedDialHomeConsumer> consumer;

- (instancetype)initWithProfile:(ProfileIOS*)profile
                  bookmarkModel:(bookmarks::BookmarkModel*)bookmarkModel;

/// Starts this mediator. Populates the speed dial folders on the top menu and
/// loads the associated items to the child pages.
- (void)startMediating;

/// Stops mediating and disconnects.
- (void)disconnect;

@end

#endif  // IOS_UI_NTP_VIVALDI_SPEED_DIAL_HOME_MEDIATOR_H_
