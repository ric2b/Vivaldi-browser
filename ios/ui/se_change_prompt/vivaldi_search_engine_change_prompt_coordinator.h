// Copyright 2025 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_SE_CHANGE_PROMPT_VIVALDI_SEARCH_ENGINE_CHANGE_PROMPT_COORDINATOR_H_
#define IOS_UI_SE_CHANGE_PROMPT_VIVALDI_SEARCH_ENGINE_CHANGE_PROMPT_COORDINATOR_H_

#import "components/search_engines/template_url_service.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"
#import "ios/ui/se_change_prompt/vivaldi_search_engine_change_prompt_type.h"

class Browser;

@protocol VivaldiSearchEngineChangePromptCoordinatorDelegate <NSObject>
- (void)coordinatorDidCloseWithSelectingPartner:(const TemplateURL*)partner;
- (void)coordinatorDidCloseWithDonateNow;
- (void)coordinatorDidCloseWithNoThanks;
@end

// This class is the coordinator for the search engine change prompt
@interface VivaldiSearchEngineChangePromptCoordinator : ChromeCoordinator

- (instancetype)
    initWithBaseViewController:(UIViewController*)viewController
                       browser:(Browser*)browser
                     providers:(const std::vector<TemplateURL*>)providers
                    promptType:(VivaldiSearchEngineChangePromptType)promptType
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@property(nonatomic, weak)
    id<VivaldiSearchEngineChangePromptCoordinatorDelegate>
        delegate;

@end

#endif  // IOS_UI_SE_CHANGE_PROMPT_VIVALDI_SEARCH_ENGINE_CHANGE_PROMPT_COORDINATOR_H_
