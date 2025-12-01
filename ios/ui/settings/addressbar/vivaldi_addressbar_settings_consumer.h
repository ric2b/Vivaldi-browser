// Copyright 2024 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_SETTINGS_ADDRESSBAR_VIVALDI_ADDRESSBAR_SETTINGS_CONSUMER_H_
#define IOS_UI_SETTINGS_ADDRESSBAR_VIVALDI_ADDRESSBAR_SETTINGS_CONSUMER_H_

#import <Foundation/Foundation.h>

// A protocol implemented by consumers to handle address bar
// preference state change.
@protocol VivaldiAddressBarSettingsConsumer

// Updates the state with the show full address preference value.
- (void)setPreferenceForShowFullAddress:(BOOL)show;
// Updates the state with the show X for suggestions preference value.
- (void)setPreferenceForShowXForSugggestions:(BOOL)show;
// Updates the state with the show typed history on focus preference value.
- (void)setPreferenceForShowTypedHistoryOnFocus:(BOOL)show;

// Updates the state with the enable bookmarks preference value.
- (void)setPreferenceForEnableBookmarks:(BOOL)enable;
// Updates the state with the prioritize bookmarks preference value.
- (void)setPreferenceForEnableBookmarksBoosted:(BOOL)enable;
// Updates the state with the enable bookmark nicknames preference value.
- (void)setPreferenceForEnableBookmarkNicknames:(BOOL)enable;
// Updates the state with the enable search suggestion preference value.
- (void)setPreferenceForEnableSearchSuggestions:(BOOL)enable;
// Updates the state with the enable history preference value.
- (void)setPreferenceForEnableSearchHistory:(BOOL)enable;
// Updates the state with the enable history preference value.
- (void)setPreferenceForEnableHistory:(BOOL)enable;
// Updates the state with the enable address bar direct match preference
// value.
- (void)setPreferenceForEnableDirectMatch:(BOOL)enable;
// Updates the state with the enable address bar direct match prioritization
// preference value.
- (void)setPreferenceForEnableDirectMatchPrioritization:(BOOL)enable;

@end

#endif  // IOS_UI_SETTINGS_ADDRESSBAR_VIVALDI_ADDRESSBAR_SETTINGS_CONSUMER_H_
