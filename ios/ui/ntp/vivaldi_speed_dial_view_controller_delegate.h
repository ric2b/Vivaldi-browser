// Copyright 2024 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_NTP_VIVALDI_SPEED_DIAL_VIEW_CONTROLLER_DELEGATE_H_
#define IOS_UI_NTP_VIVALDI_SPEED_DIAL_VIEW_CONTROLLER_DELEGATE_H_

#import "ios/ui/ntp/vivaldi_speed_dial_item.h"
#import "ios/ui/ntp/vivaldi_speed_dial_sorting_mode.h"

// SpeedDialViewControllerDelegate provides methods that allows communication
// between the SpeedDialViewController/s and the SpeedDialHomeMediator.
@protocol SpeedDialViewControllerDelegate

/// Rebuilds the speed dial folders.
- (void)computeSpeedDialFolders;

/// Rebuilds the speed dial child folders. If an item is provided construct the
/// children of that particular item, otherwise computed the children of all
/// speed dial folders.
- (void)computeSpeedDialChildItems:(VivaldiSpeedDialItem*)item;

/// Remove most visited item from top site.
- (void)removeMostVisited:(VivaldiSpeedDialItem*)item;

/// Move the selected item within same folder.
- (void)moveSpeedDialItem:(VivaldiSpeedDialItem*)item
                 position:(NSInteger)position;

/// Delete the selected item from model.
- (void)deleteSpeedDialItem:(VivaldiSpeedDialItem*)item;

@end

#endif  // IOS_UI_NTP_VIVALDI_SPEED_DIAL_VIEW_CONTROLLER_DELEGATE_H_
