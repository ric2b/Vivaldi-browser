// Copyright 2024 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_SETTINGS_APPEARANCE_VIVALDI_APPEARANCE_SETTINGS_CONSUMER_H_
#define IOS_UI_SETTINGS_APPEARANCE_VIVALDI_APPEARANCE_SETTINGS_CONSUMER_H_

#import <Foundation/Foundation.h>

// A protocol implemented by consumers to handle appearance settings
// state change.
@protocol VivaldiAppearanceSettingsConsumer

// Updates the state with the website appearance preference value.
- (void)setPreferenceWebsiteAppearanceStyle:(int)style;
// Updates the state with the force dark theme for webpage preference value.
- (void)setPreferenceWebpageForceDarkThemeEnabled:(BOOL)forceDarkThemeEnabled;
// Updates the state with the custom accent color.
- (void)setPreferenceCustomAccentColor:(NSString*)accentColor;
// Updates the state with dynamic accent color enabled preference value.
- (void)setPreferenceDynamicAccentColorEnabled:(BOOL)dynamicAccentColorEnabled;

#pragma mark: Optional methods - These do not have outbound events from UI.
// Updates the state with the bottom omnibox preference value.
@optional
- (void)setPreferenceForOmniboxAtBottom:(BOOL)bottomOmniboxEnabled;
@optional
// Updates the state with the tab bar preference value.
- (void)setPreferenceForTabBarEnabled:(BOOL)tabBarEnabled;

@end

#endif  // IOS_UI_SETTINGS_APPEARANCE_VIVALDI_APPEARANCE_SETTINGS_CONSUMER_H_
