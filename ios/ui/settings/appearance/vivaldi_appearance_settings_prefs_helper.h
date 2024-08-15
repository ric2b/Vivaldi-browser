// Copyright (c) 2023 Vivaldi Technologies AS. All rights reserved

#ifndef IOS_UI_SETTINGS_APPEARANCE_VIVALDI_APPEARANCE_SETTINGS_PREFS_HELPER_H_
#define IOS_UI_SETTINGS_APPEARANCE_VIVALDI_APPEARANCE_SETTINGS_PREFS_HELPER_H_

#import <UIKit/UIKit.h>

// TODO: @prio@vivaldi.com: Find out a better way to bridge CPP <-> Swift/UI.
// Current approach is not the efficient as it invites a duplication of CPP
// class.

@interface VivaldiAppearanceSettingsPrefsHelper : NSObject

#pragma mark - Getters
/// Returns the browser theme settings from prefs.
+ (NSString*)getBrowserTheme;
/// Returns whether browser theme is dark
+ (BOOL)isBrowserThemeDark;
/// Returns the preferred website apprearance style.
+ (int)getWebsiteAppearanceStyle;
/// Returns whether force dark theme enabled for website appearance.
+ (BOOL)forceWebsiteDarkThemeEnabled;
/// Returns the custom accent color set for toolbar/tab bar.
+ (NSString*)getCustomAccentColor;
/// Returns whether dynamic accent color fetching is enabled.
+ (BOOL)dynamicAccentColorEnabled;

#pragma mark - Setters
/// Sets the browser theme settings.
+ (void)setBrowserTheme:(NSString*)theme;
/// Sets the preferred website apprearance style.
+ (void)setWebsiteAppearanceStyle:(int)style;
/// Sets whether force dark theme enabled for website appearance.
+ (void)setForceWebsiteDarkThemeEnabled:(BOOL)enabled;
/// Sets the custom accent color set for toolbar/tab bar.
+ (void)setCustomAccentColor:(NSString*)accentColor;
/// Sets whether dynamic accent color fetching is enabled.
+ (void)setDynamicAccentColorEnabled:(BOOL)enabled;

@end

#endif  // IOS_UI_SETTINGS_APPEARANCE_VIVALDI_APPEARANCE_SETTINGS_PREFS_HELPER_H_
