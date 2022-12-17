// Copyright 2022 Vivaldi Technologies. All rights reserved.

#ifndef IOS_CHROME_BROWSER_UI_VIVALDI_SPEED_DIAL_HOME_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_VIVALDI_SPEED_DIAL_HOME_CONSUMER_H_

// SpeedDialHomeConsumer provides methods that allow mediators to update the UI.
@protocol SpeedDialHomeConsumer

/// Notifies the subscriber to refresh the top menu items.
- (void)refreshMenuItems:(NSArray*)items;

/// Notifies the subscriber to refresh the children of the speed dial folders.
- (void)refreshChildItems:(NSArray*)items;

@end

#endif  // IOS_CHROME_BROWSER_UI_VIVALDI_SPEED_DIAL_HOME_CONSUMER_H_
