// Copyright 2024 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_SETTINGS_GENERAL_VIVALDI_GENERAL_SETTINGS_CONSUMER_H_
#define IOS_UI_SETTINGS_GENERAL_VIVALDI_GENERAL_SETTINGS_CONSUMER_H_

#import <Foundation/Foundation.h>

// A protocol implemented by consumers to general settings
// preference state change.
@protocol VivaldiGeneralSettingsConsumer
// Updates the state with the homepage preference value.
- (void)setPreferenceForHomepageSwitch:(BOOL)homepageEnabled;
// Updates the home page url setting state
- (void)setPreferenceForHomepageUrl:(NSString*)url;
@end

#endif  // IOS_UI_SETTINGS_GENERAL_VIVALDI_GENERAL_SETTINGS_CONSUMER_H_
