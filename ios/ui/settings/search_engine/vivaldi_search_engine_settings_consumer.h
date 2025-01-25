// Copyright 2024 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_SETTINGS_SEARCH_ENGINE_VIVALDI_SEARCH_ENGINE_SETTINGS_CONSUMER_H_
#define IOS_UI_SETTINGS_SEARCH_ENGINE_VIVALDI_SEARCH_ENGINE_SETTINGS_CONSUMER_H_

#import <Foundation/Foundation.h>

// A protocol implemented by consumers to handle search engine
// preference state change.
@protocol VivaldiSearchEngineSettingsConsumer
// Updates the search engine for regular tabs
- (void)setSearchEngineForRegularTabs:(NSString*)searchEngine;
// Updates the search engine for private tabs
- (void)setSearchEngineForPrivateTabs:(NSString*)searchEngine;
// Updates the state with the enable search suggestion preference value.
- (void)setPreferenceForEnableSearchSuggestions:(BOOL)enable;
// Updates the state with the enable search engine nickname preference value.
- (void)setPreferenceForEnableSearchEngineNickname:(BOOL)enable;
@end

#endif  // IOS_UI_SETTINGS_SEARCH_ENGINE_VIVALDI_SEARCH_ENGINE_SETTINGS_CONSUMER_H_
