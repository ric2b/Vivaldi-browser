// Copyright 2022 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_NTP_VIVALDI_SPEED_DIAL_HOME_CONSUMER_H_
#define IOS_UI_NTP_VIVALDI_SPEED_DIAL_HOME_CONSUMER_H_

// SpeedDialHomeConsumer provides methods that allow mediators to update the UI.
@protocol SpeedDialHomeConsumer

/// Notifies the subscriber to refresh the laid out contents.
- (void)refreshContents;

/// Notifies the subscriber to refresh the top menu items.
- (void)refreshMenuItems:(NSArray*)items;

/// Notifies the subscriber to refresh the children of the speed dial folders.
- (void)refreshChildItems:(NSArray*)items;

/// Notifies the subscriber to refresh the layout when style is changed.
- (void)reloadLayout;

@end

#endif  // IOS_UI_NTP_VIVALDI_SPEED_DIAL_HOME_CONSUMER_H_
