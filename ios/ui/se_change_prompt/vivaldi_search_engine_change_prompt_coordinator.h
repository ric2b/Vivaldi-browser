// Copyright 2025 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_SE_CHANGE_PROMPT_VIVALDI_SEARCH_ENGINE_CHANGE_PROMPT_COORDINATOR_H_
#define IOS_UI_SE_CHANGE_PROMPT_VIVALDI_SEARCH_ENGINE_CHANGE_PROMPT_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

class Browser;
class TemplateURL;

@protocol VivaldiSearchEngineChangePromptCoordinatorDelegate <NSObject>
- (void)coordinatorDidCloseWithRecommendedProvider:(const TemplateURL*)provider;
- (void)coordinatorDidCloseWithCurrentProvider;
@end

// This class is the coordinator for the search engine change prompt
@interface VivaldiSearchEngineChangePromptCoordinator: ChromeCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                    recommendedProvider:(const TemplateURL*)recommendedProvider
                        currentProvider:(const TemplateURL*)currentProvider
   NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@property(nonatomic, weak)
    id<VivaldiSearchEngineChangePromptCoordinatorDelegate> delegate;

@end

#endif  // IOS_UI_SE_CHANGE_PROMPT_VIVALDI_SEARCH_ENGINE_CHANGE_PROMPT_COORDINATOR_H_
