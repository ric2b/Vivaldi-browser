// Copyright 2023 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_SETTINGS_TABS_VIVALDI_TAB_SETTINGS_CONSUMER_H_
#define IOS_UI_SETTINGS_TABS_VIVALDI_TAB_SETTINGS_CONSUMER_H_

#import <Foundation/Foundation.h>

#import "ios/ui/settings/tabs/vivaldi_ntp_type.h"

// A protocol implemented by consumers to handle address bar and tab style
// preference state change.
@protocol VivaldiTabsSettingsConsumer

// Updates the state with the bottom omnibox preference value.
- (void)setPreferenceForOmniboxAtBottom:(BOOL)omniboxAtBottom;
// Updates the state with reverse search result preference value.
- (void)setPreferenceForReverseSearchResultOrder:(BOOL)reverseOrder;
// Updates the state with the tab bar preference value.
- (void)setPreferenceForShowTabBar:(BOOL)showTabBar;
// Updates the state with open NTP on closing last tab preference value.
- (void)setPreferenceOpenNTPOnClosingLastTab:(BOOL)openNTP;
// Updates the state with focusing omnibox on NTP preference value.
- (void)setPreferenceFocusOmniboxOnNTP:(BOOL)focusOmnibox;
// Updates the state with the show X button for background tabs.
- (void)setPreferenceShowXButtonInBackgroundTab:(BOOL)showXButton;
// Updates the state with the inactive tabs threshold preference value.
- (void)setPreferenceForInactiveTabsTimeThreshold:(int)threshold;
// Updates the new tab setting state
- (void)setPreferenceForVivaldiNTPType:(VivaldiNTPType)setting
                               withURL:(NSString*)url;
@optional
// Updates the state with the show inactive tabs preference value.
- (void)setPreferenceForShowInactiveTabs:(BOOL)showInactiveTabs;
@end

#endif  // IOS_UI_SETTINGS_TABS_VIVALDI_TAB_SETTINGS_CONSUMER_H_
