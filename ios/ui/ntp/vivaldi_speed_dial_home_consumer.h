// Copyright 2022 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_NTP_VIVALDI_SPEED_DIAL_HOME_CONSUMER_H_
#define IOS_UI_NTP_VIVALDI_SPEED_DIAL_HOME_CONSUMER_H_

#import "ios/chrome/browser/bookmarks/model/legacy_bookmark_model.h"

using bookmarks::BookmarkNode;

// SpeedDialHomeConsumer provides methods that allow mediators to update the UI.
@protocol SpeedDialHomeConsumer

/// Notifies the subscriber that bookmark model is loaded.
- (void)bookmarkModelLoaded;

/// Notifies the subscriber to refresh the laid out contents.
- (void)refreshContents;

/// Notifies the subscriber to refresh the changed node.
- (void)refreshNode:(const bookmarks::BookmarkNode*)bookmarkNode;

/// Notifies the subscriber to refresh the top menu items.
- (void)refreshMenuItems:(NSArray*)items SDFolders:(NSArray*)SDFolders;

/// Notifies the subscriber to refresh the children of the speed dial folders.
- (void)refreshChildItems:(NSArray*)items;

/// Notifies the subscriber to show/hide the speed dials.
- (void)setSpeedDialsEnabled:(BOOL)enabled;

/// Notifies the subscriber to show/hide customize start page button.
- (void)setShowCustomizeStartPageButtonEnabled:(BOOL)enabled;

/// Notifies the subscriber to refresh the layout when style is changed.
- (void)reloadLayout;

@end

#endif  // IOS_UI_NTP_VIVALDI_SPEED_DIAL_HOME_CONSUMER_H_
