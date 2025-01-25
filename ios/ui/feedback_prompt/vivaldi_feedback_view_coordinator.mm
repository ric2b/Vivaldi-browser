// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#import "ios/ui/feedback_prompt/vivaldi_feedback_view_coordinator.h"

#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/ui/feedback_prompt/vivaldi_feedback_prompt_swift.h"
#import "ios/ui/feedback_prompt/vivaldi_feedback_view_delegate.h"
#import "ios/ui/feedback_prompt/vivaldi_feedback_view_mediator.h"
#import "ios/ui/vivaldi_overflow_menu/vivaldi_oveflow_menu_constants.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "vivaldi/ios/grit/vivaldi_ios_native_strings.h"

@interface VivaldiFeedbackViewCoordinator()<
                                    UIAdaptivePresentationControllerDelegate> {
  // The browser where the view is displayed.
  Browser* _browser;
  // The navigation controller that is being presented. The feedback view view
  // controller is the child of this navigation controller.
  UINavigationController* _navigationController;
}
// View provider for the feedback view/subviews.
@property(nonatomic, strong) VivaldiFeedbackViewProvider* viewProvider;
// Feedback view mediator.
@property(nonatomic, strong) VivaldiFeedbackViewMediator* mediator;
// Feedback view entry point
@property(nonatomic, assign) VivaldiFeedbackViewEntryPoint entryPoint;
// Whether view shows cancel button. Visible when presented from other
// places except main settings.
@property(nonatomic, assign) BOOL allowsCancel;

@end

@implementation VivaldiFeedbackViewCoordinator

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
      (UINavigationController*)navigationController
                                         browser:(Browser*)browser
                                      entryPoint:
                        (VivaldiFeedbackViewEntryPoint)entryPoint
                                    allowsCancel:(BOOL)allowsCancel {
  self = [self initWithBaseViewController:navigationController
                                  browser:browser
                               entryPoint:entryPoint
                             allowsCancel:allowsCancel];
  if (self) {
    _baseNavigationController = navigationController;
    _baseNavigationController.presentationController.delegate = self;
  }
  return self;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                entryPoint:
                        (VivaldiFeedbackViewEntryPoint)entryPoint
                              allowsCancel:(BOOL)allowsCancel {

  self = [super initWithBaseViewController:viewController browser:browser];

  if (self) {
    _browser = browser;
    _entryPoint = entryPoint;
    _allowsCancel = allowsCancel;
  }

  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  [self setupViewProvider];
  [self observeTapAndNavigationEvents];
  [self setupViewController];
  [self configureNavigationBarItems];
  [self setupFeedbackViewMediator];
  [self presentViewController];
}

- (void)stop {
  [super stop];
  self.viewProvider = nil;
  [self.mediator disconnect];
  self.mediator = nil;
}

#pragma mark - Actions
- (void)handleCancelButtonTap {
  [self dismiss];
}

- (void)handleRateAppTap {
  // Dismiss the view and open app store.
  [self dismiss];

  NSString *appStoreUrlString =
      [NSString stringWithUTF8String:kVivaldiAppStoreUrl];
  NSURL *appStoreUrl = [NSURL URLWithString:appStoreUrlString];

  if ([[UIApplication sharedApplication] canOpenURL:appStoreUrl]) {
    [[UIApplication sharedApplication] openURL:appStoreUrl
                                       options:@{}
                             completionHandler:nil];
  }
}

- (void)handleNoThanksTap {
  // Just dismiss the view, no extra actions needed.
  [self dismiss];
}

- (void)handleNavigateToIssueDetailWithIssue:(VivaldiFeedbackIssue*)issue {
  UIViewController *controller =
      [self.viewProvider makeIssueDetailsViewControllerWith:issue];
  controller.title = issue.titleString;

  if (self.baseNavigationController && !self.allowsCancel) {
    [self.baseNavigationController pushViewController:controller
                                             animated:YES];
  } else {
    [_navigationController pushViewController:controller
                                     animated:YES];
  }
}

- (void)handleNavigateToEditorWithParentIssue:(VivaldiFeedbackIssue*)parentIssue
                                  childIssue:(VivaldiFeedbackIssue*)childIssue {
  UIViewController *controller =
      [self.viewProvider makeFeedbackEditorViewControllerWith:parentIssue
                                                    childItem:childIssue];
  controller.title =
      childIssue != nil ? childIssue.titleString : parentIssue.titleString;

  if (self.baseNavigationController && !self.allowsCancel) {
    [self.baseNavigationController pushViewController:controller
                                             animated:YES];
  } else {
    [_navigationController pushViewController:controller
                                     animated:YES];
  }
}

- (void)handleSubmitTapWithParentIssue:(VivaldiFeedbackIssue*)parentIssue
                            childIssue:(VivaldiFeedbackIssue*)childIssue
                               message:(NSString*)message {
  if (!self.mediator)
    return;
  [self.mediator handleSubmitTapWithParentIssue:parentIssue
                                     childIssue:childIssue
                                        message:message];
}

- (void)handleFeedbackFlowCompletion {
  [self dismiss];
}

#pragma mark - Private

- (void)setupViewProvider {
  self.viewProvider = [[VivaldiFeedbackViewProvider alloc] init];
}

- (void)observeTapAndNavigationEvents {
  __weak __typeof(self) weakSelf = self;

  [self.viewProvider observeRateAppTapEvent:^{
      [weakSelf handleRateAppTap];
  }];

  [self.viewProvider observeNoThanksTapEvent:^{
      [weakSelf handleNoThanksTap];
  }];

  [self.viewProvider
      observeNavigateToIssueDetailEvent:^(VivaldiFeedbackIssue *issue) {
      [weakSelf handleNavigateToIssueDetailWithIssue:issue];
  }];

  [self.viewProvider
      observeNavigateToEditorEvent:^(VivaldiFeedbackIssue *parentIssue,
                                     VivaldiFeedbackIssue *childIssue) {
      [weakSelf handleNavigateToEditorWithParentIssue:parentIssue
                                           childIssue:childIssue];
  }];

  [self.viewProvider
      observeSubmitTapEvent:^(VivaldiFeedbackIssue *parentIssue,
                              VivaldiFeedbackIssue *childIssue,
                              NSString *message) {
      [weakSelf handleSubmitTapWithParentIssue:parentIssue
                                    childIssue:childIssue
                                       message:message];
  }];

  [self.viewProvider observeFeedbackFlowCompletion:^{
      [weakSelf handleFeedbackFlowCompletion];
  }];
}

- (void)setupViewController {
  UIViewController *controller =
      [self.viewProvider makeLandingViewControllerWith:_entryPoint];
  controller.title = l10n_util::GetNSString(IDS_IOS_FEEDBACK_MENU_TITLE);
  controller.navigationItem.largeTitleDisplayMode =
      UINavigationItemLargeTitleDisplayModeNever;
  controller.modalPresentationStyle = UIModalPresentationPageSheet;
  _navigationController =
      [[UINavigationController alloc] initWithRootViewController:controller];
  _navigationController.presentationController.delegate = self;
  [self setupSheetPresentationController];
}

- (void)configureNavigationBarItems {
  // Presented with navigation controller show cancel button on
  // left to dismiss the view.
  // When pushed within a navigation controller i.e. in Settings, show
  // a Done button instead on the right.
  if (self.allowsCancel) {
    UIBarButtonItem *cancelItem =
        [[UIBarButtonItem alloc]
            initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                                 target:self
                                 action:@selector(handleCancelButtonTap)];
    _navigationController.topViewController
        .navigationItem.leftBarButtonItem = cancelItem;
  } else {
    UIBarButtonItem *doneItem =
        [[UIBarButtonItem alloc]
            initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                                 target:self
                                 action:@selector(handleCancelButtonTap)];
    _navigationController.topViewController
          .navigationItem.rightBarButtonItem = doneItem;
  }
}

- (void)setupSheetPresentationController {
  UISheetPresentationController *sheetPc =
      _navigationController.sheetPresentationController;
  // When iPad full screen or 2/3 SplitView support only large detent because
  // medium detent cuts the contents which may make no sense at all at a glance.
  if (IsSplitToolbarMode(self.baseViewController)) {
    sheetPc.detents = @[UISheetPresentationControllerDetent.mediumDetent,
                        UISheetPresentationControllerDetent.largeDetent];
  } else {
    sheetPc.detents = @[UISheetPresentationControllerDetent.largeDetent];
  }
  sheetPc.prefersScrollingExpandsWhenScrolledToEdge = NO;
  sheetPc.widthFollowsPreferredContentSizeWhenEdgeAttached = YES;
}

- (void)setupFeedbackViewMediator {
  self.mediator = [[VivaldiFeedbackViewMediator alloc] init];
  self.mediator.consumer = self.viewProvider;
}

- (void)presentViewController {
  if (self.baseNavigationController && !self.allowsCancel) {
    [self.baseNavigationController
        pushViewController:_navigationController.topViewController
                  animated:YES];
  } else {
    [self.baseViewController presentViewController:_navigationController
                                          animated:YES
                                        completion:nil];
  }
}

#pragma mark - Helpers

- (void)dismiss {
  // Notify that the view will dismiss
  SEL willDismissSelector = @selector(feedbackViewWillDismiss);
  if ([self.delegate respondsToSelector:willDismissSelector]) {
    [self.delegate feedbackViewWillDismiss];
  }

  __weak __typeof(self) weakSelf = self;

  void (^completionBlock)(void) = ^{
    SEL didDismissSelector = @selector(feedbackViewDidDismiss);
    if ([weakSelf.delegate respondsToSelector:didDismissSelector]) {
      [weakSelf.delegate feedbackViewDidDismiss];
    }
  };

  if (self.allowsCancel) {
    [self stop];
    [self.baseViewController dismissViewControllerAnimated:YES
                                                completion:completionBlock];
  } else {
    [_baseNavigationController dismissViewControllerAnimated:YES
                                                  completion:completionBlock];
  }
}

#pragma mark - UIAdaptivePresentationControllerDelegate
- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [self dismiss];
}

@end
