// Copyright 2025 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_SE_CHANGE_PROMPT_VIVALDI_SEARCH_ENGINE_CHANGE_PROMPT_CONSUMER_H_
#define IOS_UI_SE_CHANGE_PROMPT_VIVALDI_SEARCH_ENGINE_CHANGE_PROMPT_CONSUMER_H_

#import <UIKit/UIKit.h>

// A protocol implemented by consumers to handle search engine preference.
@protocol VivaldiSearchEngineChangePromptViewConsumer

// Updates the consumer with recommended search engine
- (void)setRecommendedProvider:(NSString*)provider;

// Updates the consumer with `Description` that contains the details of
// the change. Title of the button for new search engine and title for button
// that contains old search engine.
- (void)setDescription:(NSString*)description
      recommendedProviderTitle:(NSString*)recommendedProviderTitle
          currentProviderTitle:(NSString*)currentProviderTitle;

@end

#endif  // IOS_UI_SE_CHANGE_PROMPT_VIVALDI_SEARCH_ENGINE_CHANGE_PROMPT_CONSUMER_H_
