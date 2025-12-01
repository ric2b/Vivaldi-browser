// Copyright 2025 Vivaldi Technologies. All rights reserved.

#import "ios/ui/settings/reader_mode/vivaldi_reader_mode_coordinator.h"

#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/ui/settings/reader_mode/vivaldi_reader_mode_mediator.h"
#import "ios/ui/settings/reader_mode/vivaldi_reader_mode_swift.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "vivaldi/ios/grit/vivaldi_ios_native_strings.h"

namespace {
const CGFloat kPreferredCornerRadius = 20.0;
const CGSize kPreferredPopoverSize = {420.0, 420.0};
}

@interface VivaldiReaderModeCoordinator ()
              <UISheetPresentationControllerDelegate,
               UIPopoverPresentationControllerDelegate,
               UIAdaptivePresentationControllerDelegate>
@property(nonatomic, strong) VivaldiReaderModeViewProvider* viewProvider;
// View controller for the reader mode setting.
@property(nonatomic, strong) UIViewController* viewController;
// Reader mode preference mediator.
@property(nonatomic, strong) VivaldiReaderModeMediator* mediator;
@end

@implementation VivaldiReaderModeCoordinator

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser {
  self = [super initWithBaseViewController:viewController browser:browser];
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  // Create the view provider and view controller
  self.viewProvider = [[VivaldiReaderModeViewProvider alloc] init];
  self.viewController = [VivaldiReaderModeViewProvider makeViewController];
  self.viewController.title = l10n_util::GetNSString(IDS_IOS_READER_MODE_TITLE);
  self.viewController.navigationItem.largeTitleDisplayMode =
      UINavigationItemLargeTitleDisplayModeNever;
  // Create the mediator
  self.mediator = [[VivaldiReaderModeMediator alloc]
                      initWithBrowser:self.browser];
  [self.viewProvider setConsumer:self.mediator];
  self.mediator.consumer = self.viewProvider;
    // Present the bottom sheet
  [self presentBottomSheet];
}

- (void)stop {
  [super stop];
  self.viewController = nil;
  [self.mediator disconnect];
  self.mediator = nil;
  self.viewProvider = nil;
  if (self.didStopHandler) {
    self.didStopHandler();
  }
}

#pragma mark - Private

- (void)presentBottomSheet {
  UIViewController* presentingVC = [self presentingViewController];

  if ([self isIPad]) {
    [self configureForIPadPopover];
  } else {
    self.viewController.modalPresentationStyle = UIModalPresentationPageSheet;
  }
  UISheetPresentationController* sheetPc =
      self.viewController.sheetPresentationController;
  if (sheetPc) {
    [self configureSheet:sheetPc forPresentingVC:presentingVC];
  }
  [presentingVC presentViewController:self.viewController
                             animated:YES
                           completion:^{
                              [self assignAdaptiveDelegateAfterPresentation];
                            }
  ];
}

- (BOOL)isIPad {
  return UIDevice.currentDevice.userInterfaceIdiom == UIUserInterfaceIdiomPad;
}

- (UIViewController*)presentingViewController {
  return self.baseViewController ?: self.baseNavigationController;
}

- (void)configureForIPadPopover {
  self.viewController.modalPresentationStyle = UIModalPresentationPopover;
  self.viewController.preferredContentSize = kPreferredPopoverSize;

  UIPopoverPresentationController* popoverPc =
      self.viewController.popoverPresentationController;
  if (!popoverPc) {
    return;
  }
  popoverPc.delegate = self;
  if (self.popoverSourceView && !CGRectIsEmpty(self.popoverSourceRect)) {
    popoverPc.sourceView = self.popoverSourceView;
    popoverPc.sourceRect = self.popoverSourceRect;
    popoverPc.permittedArrowDirections = UIPopoverArrowDirectionAny;
  }
}

- (void)configureSheet:(UISheetPresentationController*)sheetPc
      forPresentingVC:(UIViewController*)presentingVC {
  sheetPc.detents = @[ UISheetPresentationControllerDetent.mediumDetent ];
  sheetPc.selectedDetentIdentifier = sheetPc.detents.firstObject.identifier;
  sheetPc.preferredCornerRadius = kPreferredCornerRadius;
  sheetPc.delegate = self;
}

- (void)assignAdaptiveDelegateAfterPresentation {
  UIPresentationController* pc = self.viewController.presentationController;
  if (pc) {
    pc.delegate = self;
  }
}

#pragma mark - UISheetPresentationControllerDelegate

- (void)presentationControllerDidDismiss:(UIPresentationController*)
                                            presentationController {
  [self stop];
}

#pragma mark - UIPopoverPresentationControllerDelegate

- (void)popoverPresentationControllerDidDismissPopover:
    (UIPopoverPresentationController *)popoverPresentationController {
  [self stop];
}

@end
