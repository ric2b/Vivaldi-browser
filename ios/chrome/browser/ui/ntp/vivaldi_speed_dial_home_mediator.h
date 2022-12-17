// Copyright 2022 Vivaldi Technologies. All rights reserved.

#ifndef IOS_CHROME_BROWSER_UI_VIVALDI_SPEED_DIAL_HOME_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_VIVALDI_SPEED_DIAL_HOME_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/ntp/vivaldi_speed_dial_home_consumer.h"
#import "ios/chrome/browser/ui/ntp/vivaldi_speed_dial_item.h"
#import "ios/chrome/browser/ui/ntp/vivaldi_speed_dial_sorting_mode.h"
#include "components/bookmarks/browser/bookmark_model.h"

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

class ChromeBrowserState;
using bookmarks::BookmarkModel;

// VivaldiSpeedDialHomeMediator manages model interactions for the
// VivaldiNewTabPageViewController.
@interface VivaldiSpeedDialHomeMediator : NSObject

@property(nonatomic, weak) id<SpeedDialHomeConsumer> consumer;

- (instancetype)initWithBrowserState:(ChromeBrowserState*)browserState
                       bookmarkModel:(BookmarkModel*)bookmarkModel;

/// Starts this mediator. Populates the speed dial folders on the top menu and
/// loads the associated items to the child pages.
- (void)startMediating;

/// Stops mediating and disconnects.
- (void)disconnect;

/// Rebuilds the speed dial folders.
- (void)computeSpeedDialFolders;

/// Rebuilds the speed dial child folders. If an item is provided construct the
/// children of that particular item, otherwise computed the children of all
/// speed dial folders.
- (void)computeSpeedDialChildItems:(VivaldiSpeedDialItem*)item;

/// Computes the sorted child items based on mode and notifies the consumer.
- (void)computeSortedItems:(NSMutableArray*)items
                    byMode:(SpeedDialSortingMode)mode;

@end

#endif  // IOS_CHROME_BROWSER_UI_VIVALDI_SPEED_DIAL_HOME_MEDIATOR_H_
