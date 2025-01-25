// Copyright 2024 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_SETTINGS_ADDRESSBAR_VIVALDI_ADDRESSBAR_SETTINGS_CONSUMER_H_
#define IOS_UI_SETTINGS_ADDRESSBAR_VIVALDI_ADDRESSBAR_SETTINGS_CONSUMER_H_

#import <Foundation/Foundation.h>

// A protocol implemented by consumers to handle address bar
// preference state change.
@protocol VivaldiAddressBarSettingsConsumer

// Updates the state with the show full address preference value.
- (void)setPreferenceForShowFullAddress:(BOOL)show;
// Updates the state with the enable bookmarks matching preference value.
- (void)setPreferenceForEnableBookmarksMatching:(BOOL)enableMatching;
// Updates the state with the enable bookmarks nickname matching preference
// value.
- (void)setPreferenceForEnableBookmarksNicknameMatching:(BOOL)enableMatching;
// Updates the state with the enable address bar direct match preference
// value.
- (void)setPreferenceForEnableDirectMatch:(BOOL)enable;
// Updates the state with the enable address bar direct match prioritization
// preference value.
- (void)setPreferenceForEnableDirectMatchPrioritization:(BOOL)enable;

@end

#endif  // IOS_UI_SETTINGS_ADDRESSBAR_VIVALDI_ADDRESSBAR_SETTINGS_CONSUMER_H_
