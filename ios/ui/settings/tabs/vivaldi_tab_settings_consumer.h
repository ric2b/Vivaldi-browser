// Copyright 2023 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_SETTINGS_TABS_VIVALDI_TAB_SETTINGS_CONSUMER_H_
#define IOS_UI_SETTINGS_TABS_VIVALDI_TAB_SETTINGS_CONSUMER_H_

#import <Foundation/Foundation.h>

// A protocol implemented by consumers to handle address bar and tab style
// preference state change.
@protocol VivaldiTabsSettingsConsumer

// Updates the state with the bottom omnibox preference value.
- (void)setPreferenceForOmniboxAtBottom:(BOOL)omniboxAtBottom;
// Updates the state with reverse search result preference value.
- (void)setPreferenceForReverseSearchResultOrder:(BOOL)reverseOrder;
// Updates the state with the tab bar preference value.
- (void)setPreferenceForShowTabBar:(BOOL)showTabBar;
// Updates the state with the inactive tabs threshold preference value.
- (void)setPreferenceForInactiveTabsTimeThreshold:(int)threshold;
// Updates the state with the show inactive tabs preference value.
@optional
- (void)setPreferenceForShowInactiveTabs:(BOOL)showInactiveTabs;

@end

#endif  // IOS_UI_SETTINGS_TABS_VIVALDI_TAB_SETTINGS_CONSUMER_H_
