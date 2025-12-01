// Copyright 2025 Vivaldi Technologies. All rights reserved.

#import "ios/ui/se_change_prompt/vivaldi_search_engine_change_prompt_coordinator.h"

#import "components/search_engines/template_url.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/ui/device_orientation/scoped_force_portrait_orientation.h"
#import "ios/ui/se_change_prompt/search_engine_change_prompt_swift.h"
#import "ios/ui/se_change_prompt/vivaldi_search_engine_change_prompt_mediator.h"

@interface VivaldiSearchEngineChangePromptCoordinator() {
  const TemplateURL* _recommendedProvider;
  const TemplateURL* _currentProvider;

  /// Forces the device orientation in portrait mode.
  std::unique_ptr<ScopedForcePortraitOrientation> _scopedForceOrientation;
}

// View provider for the settings page.
@property(nonatomic, strong)
    VivaldiSearchEngineChangePromptViewProvider* viewProvider;
// View controller for the setting page.
@property(nonatomic, strong) UIViewController* controller;
// Mediator for the setting
@property(nonatomic, strong) VivaldiSearchEngineChangePromptMediator* mediator;
// Navigation controller where settings controller is presented
@property(nonatomic, strong) UINavigationController* navigationController;
@end

@implementation VivaldiSearchEngineChangePromptCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                    recommendedProvider:(const TemplateURL*)recommendedProvider
                        currentProvider:(const TemplateURL*)currentProvider {
  self = [super initWithBaseViewController:viewController
                                   browser:browser];
  if (self) {
    _recommendedProvider = recommendedProvider;
    _currentProvider = currentProvider;
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  [self lockOrientationInPortrait:YES];

  VivaldiSearchEngineChangePromptViewProvider* viewProvider =
      [[VivaldiSearchEngineChangePromptViewProvider alloc] init];
  self.viewProvider = viewProvider;
  self.controller = [self.viewProvider makeViewController];
  self.controller.modalInPresentation = true;
  self.mediator =
      [[VivaldiSearchEngineChangePromptMediator alloc]
          initWithRecommendedProvider:_recommendedProvider
              currentProvider:_currentProvider];
  self.mediator.consumer = self.viewProvider;

  UINavigationController* navigationController =
      [[UINavigationController alloc]
          initWithRootViewController:self.controller];
  self.navigationController = navigationController;

  UISheetPresentationController* sheetPc =
      navigationController.sheetPresentationController;
  sheetPc.detents = @[UISheetPresentationControllerDetent.mediumDetent];
  sheetPc.prefersScrollingExpandsWhenScrolledToEdge = NO;
  sheetPc.widthFollowsPreferredContentSizeWhenEdgeAttached = YES;

  [self.baseViewController presentViewController:navigationController
                                        animated:YES
                                      completion:nil];

  [self observeTapEvents];
}

- (void)stop {
  [super stop];
  [self lockOrientationInPortrait:NO];

  [self.mediator disconnect];
  self.mediator = nil;

  self.viewProvider = nil;
  self.controller = nil;
  self.navigationController = nil;

  _recommendedProvider = nullptr;
  _currentProvider = nullptr;
}

#pragma mark - Private
- (void)observeTapEvents {
  __weak __typeof(self) weakSelf = self;

  [self.viewProvider observeUseRecommendedProviderTapEvent:^{
    __strong __typeof(weakSelf) strongSelf = weakSelf;
    if (strongSelf) {
      [strongSelf.baseViewController dismissViewControllerAnimated:YES
                                                        completion:^{
        [strongSelf.delegate
            coordinatorDidCloseWithRecommendedProvider:
                strongSelf->_recommendedProvider];
      }];
    }
  }];

  [self.viewProvider observeUseCurrentProviderTapEvent:^{
    [weakSelf.baseViewController dismissViewControllerAnimated:YES
                                                      completion:^{
      [weakSelf.delegate coordinatorDidCloseWithCurrentProvider];
    }];
  }];
}

- (void)lockOrientationInPortrait:(BOOL)portraitLock {
  AppState* appState = self.browser->GetSceneState().profileState.appState;
  if (portraitLock) {
    if (!appState) {
      return;
    }
    _scopedForceOrientation = ForcePortraitOrientationOnIphone(appState);
  } else {
    _scopedForceOrientation.reset();
  }
}

@end
