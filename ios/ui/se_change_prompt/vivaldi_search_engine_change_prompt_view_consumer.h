// Copyright 2025 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_SE_CHANGE_PROMPT_VIVALDI_SEARCH_ENGINE_CHANGE_PROMPT_CONSUMER_H_
#define IOS_UI_SE_CHANGE_PROMPT_VIVALDI_SEARCH_ENGINE_CHANGE_PROMPT_CONSUMER_H_

#import <UIKit/UIKit.h>
#import "ios/ui/se_change_prompt/vivaldi_search_engine_change_prompt_type.h"

@class VivaldiSearchEngineChangePromptPartnerItem;

// A protocol implemented by consumers to handle search engine preference.
@protocol VivaldiSearchEngineChangePromptViewConsumer

// Updates the consumer with recommended search engines
- (void)setRecommendedProviders:
            (NSArray<VivaldiSearchEngineChangePromptPartnerItem*>*)partners
                     promptType:(VivaldiSearchEngineChangePromptType)promptType;

@end

#endif  // IOS_UI_SE_CHANGE_PROMPT_VIVALDI_SEARCH_ENGINE_CHANGE_PROMPT_CONSUMER_H_