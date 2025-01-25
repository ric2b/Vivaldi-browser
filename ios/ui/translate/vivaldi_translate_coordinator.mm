// Copyright 2024 Vivaldi Technologies. All rights reserved.

#import "ios/ui/translate/vivaldi_translate_coordinator.h"

#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/ui/helpers/vivaldi_global_helpers.h"
#import "ios/ui/translate/vivaldi_translate_mediator.h"
#import "ios/ui/translate/vivaldi_translate_swift.h"
#import "ui/base/l10n/l10n_util.h"
#import "vivaldi/ios/grit/vivaldi_ios_native_strings.h"

namespace {
// If input text goes over this limit we open full sheet since
// the text is too long for half sheet.
NSUInteger halfSheetCharsThreshold = 300;
}

@interface VivaldiTranslateCoordinator()
                              <UIAdaptivePresentationControllerDelegate,
                              UIPopoverPresentationControllerDelegate> {
}
// The parent view controller where this view is presented. This can be
// different than the baseViewController. This aligns with the active window
// from where currently active root modal view controller is presented.
@property (nonatomic, strong) UIViewController* presentingViewController;

// View provider for the translate scene.
@property(nonatomic, strong) VivaldiTranslateViewProvider* viewProvider;

// View controller for the translate scene.
@property(nonatomic, strong) UIViewController* controller;

// View controller for the translate scene editor for iPad side panel.
@property(nonatomic, strong) UIViewController* sidePanelEditorController;

// Navigation controller for the translate scene editor for iPad side panel.
@property(nonatomic, strong) UIViewController* sidePanelEditorNavController;

// Mediator for the scene
@property(nonatomic, strong) VivaldiTranslateMediator* mediator;

// View from where translation view is presented
@property(nonatomic, weak) UIView* originView;

// Rect of the view from where translation view is presented
@property(nonatomic, assign) CGRect originRect;

// Entry point for the translation view i.e Panel, ContextMenu
@property(nonatomic, assign) VivaldiTranslateEntryPoint entryPoint;

// Text with which translate view is presented. NIL when opened from panel.
@property(nonatomic, strong) NSString* selectedText;

@end

@implementation VivaldiTranslateCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                  presentingViewController:
        (UIViewController*)presentingViewController
                                 browser:(Browser*)browser
                              entryPoint:(VivaldiTranslateEntryPoint)entryPoint
                            selectedText:(NSString*)selectedText
                              originView:(UIView*)originView
                              originRect:(CGRect)originRect {

  self = [super initWithBaseViewController:viewController
                                   browser:browser];
  if (self) {
    _presentingViewController = presentingViewController;
    _selectedText = selectedText;
    _originView = originView;
    _originRect = originRect;
    _entryPoint = entryPoint;
  }
  return self;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                  presentingViewController:
        (UIViewController*)presentingViewController
                                   browser:(Browser*)browser
                          entryPoint:(VivaldiTranslateEntryPoint)entryPoint
                              selectedText:(NSString*)selectedText {

  return [self initWithBaseViewController:viewController
                 presentingViewController:presentingViewController
                                  browser:browser
                               entryPoint:entryPoint
                             selectedText:selectedText
                               originView:nil
                               originRect:CGRectZero];
}

#pragma mark - ChromeCoordinator

- (void)start {
  [self setupViewProvider];
  [self observeTapAndNavigationEvents];
  [self setupViewController];
  [self setupViewMediator];
}

- (void)stop {
  [super stop];
  [self.mediator disconnect];
  self.mediator = nil;

  self.viewProvider = nil;
  self.controller = nil;
  self.presentingViewController = nil;
  self.sidePanelEditorController = nil;
  self.navigationController = nil;
  self.sidePanelEditorNavController = nil;

  self.selectedText = nil;
}

- (BOOL)shouldOpenFullSheet {
  if ([_selectedText length] <= 0)
    return NO;

  __block NSUInteger characterCount = 0;

  [_selectedText
      enumerateSubstringsInRange:NSMakeRange(0, [_selectedText length])
                        options:NSStringEnumerationByComposedCharacterSequences
                     usingBlock:^(NSString *substring,
                                  NSRange substringRange,
                                  NSRange enclosingRange,
                                  BOOL *stop) {
      characterCount++;
  }];

  return characterCount > halfSheetCharsThreshold;
}

#pragma mark - Actions

- (void)handleCloseButtonTap {
  [self stop];

  if (self.entryPoint == VivaldiTranslateEntryPointContextMenu) {
    [self.baseViewController dismissViewControllerAnimated:YES
                                                completion:nil];
  } else {
    [self.panelDelegate panelDismissed];
    self.panelDelegate = nil;
  }

  [self.delegate translateViewDidDismiss];
  self.delegate = nil;
}

- (void)closeSidePanelEditor {
  [self.sidePanelEditorNavController dismissViewControllerAnimated:YES
                                                        completion:nil];
  self.sidePanelEditorController = nil;
  self.sidePanelEditorNavController = nil;
}

- (void)handleTabletEditorFocusEvent {
  UIViewController *controller =
      [self.viewProvider
          makeTabletEditorViewControllerWithPresentingViewControllerSize:
              self.presentingViewController.view.frame.size];
  controller.title = l10n_util::GetNSString(IDS_VIVALDI_TRANSLATE_TITLE);
  self.sidePanelEditorController = controller;

  UINavigationController* navigationController =
      [[UINavigationController alloc] initWithRootViewController:controller];
  self.sidePanelEditorNavController = navigationController;
  UIBarButtonItem *doneItem =
      [[UIBarButtonItem alloc]
          initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                               target:self
                               action:@selector(closeSidePanelEditor)];
  navigationController.topViewController
      .navigationItem.rightBarButtonItem = doneItem;

  [self configureTransparentNavBarForController:navigationController];

  [self.baseViewController presentViewController:navigationController
                                        animated:YES
                                      completion:nil];
}

#pragma mark - Private

- (void)setupViewProvider {
  self.viewProvider = [[VivaldiTranslateViewProvider alloc] init];
}

- (void)observeTapAndNavigationEvents {
  __weak __typeof(self) weakSelf = self;

  [self.viewProvider observeCloseButtonTapEvent:^{
    [weakSelf handleCloseButtonTap];
  }];

  [self.viewProvider observeTapOnSidePanelInputFieldEvent:^{
    [weakSelf handleTabletEditorFocusEvent];
  }];
}

- (void)setupViewController {
  UIViewController *controller =
      [self.viewProvider makeViewControllerWith:_entryPoint];
  self.controller = controller;

  // When entry point is panel wrap the controller with a navigation controller.
  if (self.entryPoint == VivaldiTranslateEntryPointPanel ||
      self.entryPoint == VivaldiTranslateEntryPointSidePanel) {
    self.controller.title = l10n_util::GetNSString(IDS_VIVALDI_TRANSLATE_TITLE);
    self.controller.view.backgroundColor =
        [UIColor colorNamed:kGroupedPrimaryBackgroundColor];
    self.navigationController =
        [[UINavigationController alloc] initWithRootViewController:controller];
    self.navigationController.presentationController.delegate = self;

    [self configureTransparentNavBarForController:self.navigationController];

    [self configureNavigationBarItems];
  } else {
    self.controller.modalPresentationStyle = UIModalPresentationPopover;

    UIPopoverPresentationController* popoverPresentationController =
        self.controller.popoverPresentationController;
    popoverPresentationController.sourceView = _originView;
    popoverPresentationController.sourceRect = _originRect;
    popoverPresentationController.permittedArrowDirections =
        UIPopoverArrowDirectionUp | UIPopoverArrowDirectionDown;

    popoverPresentationController.delegate = self;
    popoverPresentationController.backgroundColor =
        [UIColor colorNamed:kGroupedPrimaryBackgroundColor];

    [self setupSheetForViewController:self.controller];

    [self.baseViewController presentViewController:self.controller
                                          animated:YES
                                        completion:nil];
  }
}

- (void)setupSheetForViewController:(UIViewController*)viewController {
  // The adaptive controller adjusts styles based on window size: sheet
  // for slim windows on iPhone and iPad, popover for larger windows on
  // iPad.
  UISheetPresentationController* sheetPresentationController =
      viewController.popoverPresentationController
          .adaptiveSheetPresentationController;
  if (!sheetPresentationController) {
    return;
  }

  void (^changes)(void) = ^{
    sheetPresentationController.prefersEdgeAttachedInCompactHeight = YES;
    sheetPresentationController
        .widthFollowsPreferredContentSizeWhenEdgeAttached = YES;

    NSArray<UISheetPresentationControllerDetent*>* regularDetents = @[
      [UISheetPresentationControllerDetent mediumDetent],
      [UISheetPresentationControllerDetent largeDetent]
    ];

    NSArray<UISheetPresentationControllerDetent*>* largeTextDetents =
        @[ [UISheetPresentationControllerDetent largeDetent] ];

    BOOL hasLargeText = UIContentSizeCategoryIsAccessibilityCategory(
         viewController.traitCollection.preferredContentSizeCategory) ||
              [self shouldOpenFullSheet];
    sheetPresentationController.detents =
        hasLargeText ? largeTextDetents : regularDetents;
  };

  changes();
}

- (void)configureNavigationBarItems {
  // When presented on panel with navigation controller show
  // a Done button on the right.
  UIBarButtonItem *doneItem =
      [[UIBarButtonItem alloc]
          initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                               target:self
                               action:@selector(handleCloseButtonTap)];
  _navigationController.topViewController
      .navigationItem.rightBarButtonItem = doneItem;
}

- (void)configureTransparentNavBarForController:
    (UINavigationController*)navigationController {
  UINavigationBarAppearance* transparentAppearance =
      [[UINavigationBarAppearance alloc] init];
  [transparentAppearance configureWithTransparentBackground];
  navigationController.navigationBar.standardAppearance =
      transparentAppearance;
  navigationController.navigationBar.compactAppearance =
      transparentAppearance;
  navigationController.navigationBar.scrollEdgeAppearance =
      transparentAppearance;
}

- (void)setupViewMediator {
  ProfileIOS *profile =
      self.browser->GetProfile()->GetOriginalProfile();
  self.mediator =
      [[VivaldiTranslateMediator alloc] initWithSelectedText:_selectedText
                                                     profile:profile];
  self.mediator.consumer = self.viewProvider;

  self.viewProvider.delegate = self.mediator;
}

#pragma mark - UIAdaptivePresentationControllerDelegate
- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [self handleCloseButtonTap];
}

- (void)popoverPresentationControllerDidDismissPopover:
    (UIPopoverPresentationController*)popoverPresentationController {
  [self handleCloseButtonTap];
}

@end
