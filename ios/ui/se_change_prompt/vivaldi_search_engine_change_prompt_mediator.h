// Copyright 2025 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_SE_CHANGE_PROMPT_VIVALDI_SEARCH_ENGINE_CHANGE_PROMPT_MEDIATOR_H_
#define IOS_UI_SE_CHANGE_PROMPT_VIVALDI_SEARCH_ENGINE_CHANGE_PROMPT_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/ui/se_change_prompt/vivaldi_search_engine_change_prompt_view_consumer.h"

class TemplateURL;

// The mediator for search engine change prompt settings.
@interface VivaldiSearchEngineChangePromptMediator: NSObject

- (instancetype)initWithRecommendedProvider:
                                (const TemplateURL*)recommendedProvider
                            currentProvider:(const TemplateURL*)currentProvider
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@property(nonatomic, weak)
    id<VivaldiSearchEngineChangePromptViewConsumer> consumer;

- (void)disconnect;

@end

#endif  // IOS_UI_SE_CHANGE_PROMPT_VIVALDI_SEARCH_ENGINE_CHANGE_PROMPT_MEDIATOR_H_
