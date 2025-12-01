// Copyright 2025 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_SE_CHANGE_PROMPT_VIVALDI_SEARCH_ENGINE_CHANGE_PROMPT_MEDIATOR_H_
#define IOS_UI_SE_CHANGE_PROMPT_VIVALDI_SEARCH_ENGINE_CHANGE_PROMPT_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "components/search_engines/template_url_service.h"
#import "ios/ui/se_change_prompt/vivaldi_search_engine_change_prompt_type.h"
#import "ios/ui/se_change_prompt/vivaldi_search_engine_change_prompt_view_consumer.h"

class TemplateURL;

// The mediator for search engine change prompt settings.
@interface VivaldiSearchEngineChangePromptMediator : NSObject

- (instancetype)initWithProviders:(const std::vector<TemplateURL*>)providers
                       promptType:
                           (VivaldiSearchEngineChangePromptType)promptType
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@property(nonatomic, weak) id<VivaldiSearchEngineChangePromptViewConsumer>
    consumer;

- (void)disconnect;

@end

#endif  // IOS_UI_SE_CHANGE_PROMPT_VIVALDI_SEARCH_ENGINE_CHANGE_PROMPT_MEDIATOR_H_
