// Copyright 2025 Vivaldi Technologies. All rights reserved.

#import "ios/ui/se_change_prompt/vivaldi_search_engine_change_prompt_coordinator.h"

#import "base/strings/sys_string_conversions.h"
#import "components/search_engines/template_url.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/device_orientation/ui_bundled/scoped_force_portrait_orientation.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/ui/se_change_prompt/search_engine_change_prompt_swift.h"
#import "ios/ui/se_change_prompt/vivaldi_search_engine_change_prompt_mediator.h"

// Finds a search engine provider by partner ID and short name.
static TemplateURL* FindProvider(const std::vector<TemplateURL*>& providers,
                                 int64_t partnerId,
                                 NSString* shortName) {
  const std::u16string short_name = base::SysNSStringToUTF16(shortName);

  auto it =
      std::find_if(providers.begin(), providers.end(), [&](TemplateURL* p) {
        return p && p->id() == partnerId && p->short_name() == short_name;
      });

  return (it == providers.end()) ? nullptr : *it;
}

namespace {
NSString* customDetentIdentifier = @"VivaldiSearchEnginePromptDetent";
}

@interface VivaldiSearchEngineChangePromptCoordinator () {
  // Available search engine providers.
  std::vector<TemplateURL*> _providers;

  // Forces device orientation in portrait mode.
  std::unique_ptr<ScopedForcePortraitOrientation> _scopedForceOrientation;

  // Custom sheet detent identifier for dynamic height.
  UISheetPresentationControllerDetentIdentifier _customDetentIdentifier;
}

@property(nonatomic, assign) VivaldiSearchEngineChangePromptType promptType;
@property(nonatomic, strong)
    VivaldiSearchEngineChangePromptViewProvider* viewProvider;
@property(nonatomic, strong) UIViewController* controller;
@property(nonatomic, strong) VivaldiSearchEngineChangePromptMediator* mediator;
@property(nonatomic, strong) UINavigationController* navigationController;

@end

@implementation VivaldiSearchEngineChangePromptCoordinator

- (instancetype)
    initWithBaseViewController:(UIViewController*)viewController
                       browser:(Browser*)browser
                     providers:(const std::vector<TemplateURL*>)providers
                    promptType:(VivaldiSearchEngineChangePromptType)promptType {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _promptType = promptType;
    _providers = providers;
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  [self lockOrientationInPortrait:YES];
  [self setupViewProvider];
  [self setupMediator];
  [self configureSheetPresentation];
  [self presentViewController];
  [self observeTapEvents];
}

- (void)stop {
  [super stop];
  [self lockOrientationInPortrait:NO];
  [self cleanupResources];
}

#pragma mark - Private Setup Methods

- (void)setupViewProvider {
  self.viewProvider =
      [[VivaldiSearchEngineChangePromptViewProvider alloc] init];
  self.controller =
      [self.viewProvider makeViewControllerWithPresentingViewControllerTrait:
            self.baseViewController.traitCollection];
  self.controller.modalInPresentation = true;
}

- (void)setupMediator {
  self.mediator = [[VivaldiSearchEngineChangePromptMediator alloc]
      initWithProviders:_providers
             promptType:_promptType];
  self.mediator.consumer = self.viewProvider;
}

- (void)configureSheetPresentation {

  UISheetPresentationController* sheetPc =
      self.controller.sheetPresentationController;

  // Create dynamic height resolver based on content.
  auto detentResolver = ^CGFloat(
      id<UISheetPresentationControllerDetentResolutionContext> context) {
    return self.viewProvider.contentHeight;
  };

  UISheetPresentationControllerDetent* initialDetent =
      [UISheetPresentationControllerDetent
          customDetentWithIdentifier:customDetentIdentifier
                            resolver:detentResolver];

  sheetPc.detents = @[ initialDetent ];
  sheetPc.prefersScrollingExpandsWhenScrolledToEdge = NO;
  sheetPc.widthFollowsPreferredContentSizeWhenEdgeAttached = YES;

  [self setupHeightChangeHandler];
}

- (void)setupHeightChangeHandler {
  __weak __typeof(self) weakSelf = self;
  self.viewProvider.heightDidChange = ^(CGFloat newHeight) {
    __strong __typeof(weakSelf) strongSelf = weakSelf;
    if (strongSelf && strongSelf.controller.sheetPresentationController) {
      // Animate sheet height changes when content size changes.
      [strongSelf.controller.sheetPresentationController invalidateDetents];
      [strongSelf.controller.sheetPresentationController animateChanges:^{
        strongSelf.controller.sheetPresentationController
            .selectedDetentIdentifier = customDetentIdentifier;
      }];
    }
  };
}

- (void)presentViewController {
  [self.baseViewController presentViewController:self.controller
                                        animated:YES
                                      completion:nil];
}

- (void)cleanupResources {
  [self.mediator disconnect];
  self.mediator = nil;
  self.viewProvider = nil;
  self.controller = nil;
  self.navigationController = nil;
  _providers.clear();
}

#pragma mark - Event Handling

- (void)observeTapEvents {
  __weak __typeof(self) weakSelf = self;

  [self observeConfirmChoiceEvent:weakSelf];
  [self observeDonateNowEvent:weakSelf];
  [self observeNoThanksEvent:weakSelf];
}

- (void)observeConfirmChoiceEvent:
    (__weak __typeof(VivaldiSearchEngineChangePromptCoordinator*))weakSelf {
  [self.viewProvider
      observeConfirmChoiceTapEvent:^(
          VivaldiSearchEngineChangePromptPartnerItem* _Nullable selectedPartner) {
        __strong __typeof(weakSelf) strongSelf = weakSelf;
        if (!strongSelf || !selectedPartner)
          return;

        [strongSelf.baseViewController
            dismissViewControllerAnimated:YES
                               completion:^{
                                 __strong __typeof(weakSelf) innerSelf =
                                     weakSelf;
                                 if (!innerSelf)
                                   return;

                                 // Find the selected provider and notify
                                 // delegate.
                                 TemplateURL* provider =
                                     FindProvider(innerSelf->_providers,
                                                  selectedPartner.partnerId,
                                                  selectedPartner.shortName);

                                 if (provider) {
                                   [innerSelf.delegate
                                       coordinatorDidCloseWithSelectingPartner:
                                           provider];
                                 } else {
                                   // Fallback: should never happen.
                                   [innerSelf.delegate
                                           coordinatorDidCloseWithNoThanks];
                                 }
                               }];
      }];
}

- (void)observeDonateNowEvent:
    (__weak __typeof(VivaldiSearchEngineChangePromptCoordinator*))weakSelf {
  [self.viewProvider observeDonateNowTapEvent:^{
    [weakSelf.baseViewController
        dismissViewControllerAnimated:YES
                           completion:^{
                             [weakSelf.delegate
                                     coordinatorDidCloseWithDonateNow];
                           }];
  }];
}

- (void)observeNoThanksEvent:
    (__weak __typeof(VivaldiSearchEngineChangePromptCoordinator*))weakSelf {
  [self.viewProvider observeNoThanksTapEvent:^{
    [weakSelf.baseViewController
        dismissViewControllerAnimated:YES
                           completion:^{
                             [weakSelf
                                     .delegate coordinatorDidCloseWithNoThanks];
                           }];
  }];
}

#pragma mark - Orientation Management

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
