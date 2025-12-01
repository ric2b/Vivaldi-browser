// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#import "ios/ui/settings/reader_mode/vivaldi_reader_mode_floating_ui.h"

#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/ui/settings/reader_mode/vivaldi_reader_mode_animator.h"
#import "ios/ui/settings/reader_mode/vivaldi_reader_mode_coordinator.h"

namespace {
const CGFloat kButtonSize = 46.0;
const CGFloat kButtonCornerRadius = 23.0;
const CGFloat kButtonMargin = 16.0;
const NSTimeInterval kFadeDuration = 0.2;
const CGFloat kShadowOpacity = 1.0;
const CGFloat kShadowRadius = 8.0;
const CGFloat kShadowOffsetWidth = 0.0;
const CGFloat kShadowOffsetHeight = 2.0;
const char kSymbolName[] = "readerview_settings";
const char kAccessibilityLabel[] = "Reader mode settings";
UIColor* ButtonBackgroundColor() {
  return [UIColor colorWithWhite:1.0 alpha:0.95];
}
UIColor* ShadowUIColor() {
  return [UIColor colorWithWhite:0 alpha:0.25];
}
}

@interface VivaldiReaderModeFloatingUI ()
@property(nonatomic, copy) UIView* (^containerProvider)(void);
@property(nonatomic, strong) UIButton* button;
@property(nonatomic, strong) VivaldiReaderModeCoordinator* coordinator;
@property(nonatomic, assign) Browser* browser;
@end

@implementation VivaldiReaderModeFloatingUI

- (instancetype)initWithContainerProvider:(UIView* (^)(void))containerProvider {
  if ((self = [super init])) {
    // Copying the provider block so it is heap-allocated and safely retained
    // beyond the caller's scope. Blocks can be stack-based when passed in;
    // copying ensures correct lifetime under ARC.
    _containerProvider = [containerProvider copy];
  }
  return self;
}

- (void)setBrowserPointer:(void*)browserPtr {
  _browser = (Browser*)browserPtr;
}

- (void)configureWithBrowserPointer:(void*)browserPtr visible:(BOOL)visible {
  [self setBrowserPointer:browserPtr];
  [self setupContainer];
  [self updateVisible:visible];
}

- (void)presentFromAnchor:(UIView*)anchor {
  if (!self.browser) return;
  if (self.coordinator) {
    [self.coordinator stop];
    self.coordinator = nil;
  }
  // Ensure UI exists before presenting.
  [self setupContainer];
  UIViewController* baseVC = [self baseViewControllerForAnchor:anchor];
  VivaldiReaderModeCoordinator* coordinator =
      [[VivaldiReaderModeCoordinator alloc]
                          initWithBaseViewController:baseVC
                                             browser:self.browser];
  coordinator.popoverSourceView = anchor ?: self.button;
  coordinator.popoverSourceRect = (anchor ?: self.button).bounds;
  __weak VivaldiReaderModeFloatingUI* weakSelf = self;
  coordinator.didStopHandler = ^{
    VivaldiReaderModeFloatingUI* strongSelf = weakSelf;
    if (strongSelf) {
      strongSelf.coordinator = nil;
    }
  };
  self.coordinator = coordinator;
  [self.coordinator start];
}


- (void)invalidate {
  if (self.button) {
    [self.button removeFromSuperview];
    self.button = nil;
  }
  self.coordinator = nil;
}

#pragma mark - Private methods / helpers

- (void)setupContainer {
  if (!self.containerProvider) return;
  UIView* container = self.containerProvider();
  if (!container) return;
  if (!self.button) {
    [self setupViewsInContainer:container];
  } else if (self.button.superview != container) {
    [self.button removeFromSuperview];
    [container addSubview:self.button];
  }
}

- (void)setupViewsInContainer:(UIView*)container {
  UIButton* button = [UIButton buttonWithType:UIButtonTypeSystem];
  button.translatesAutoresizingMaskIntoConstraints = NO;
  button.backgroundColor = ButtonBackgroundColor();
  button.layer.cornerRadius = kButtonCornerRadius;
  button.layer.shadowColor = ShadowUIColor().CGColor;
  button.layer.shadowOpacity = kShadowOpacity;
  button.layer.shadowRadius = kShadowRadius;
  button.layer.shadowOffset = CGSizeMake(
      kShadowOffsetWidth,
      kShadowOffsetHeight);
  UIImage* symbol = [UIImage imageNamed:@(kSymbolName)];
  [button setImage:symbol forState:UIControlStateNormal];
  // keeping the same tint color as the symbol for dark mode and light mode
  button.tintColor = [UIColor blackColor];
  button.accessibilityLabel = @(kAccessibilityLabel);
  [button.widthAnchor constraintEqualToConstant:kButtonSize].active = YES;
  [button.heightAnchor constraintEqualToConstant:kButtonSize].active = YES;

  [container addSubview:button];
  UILayoutGuide* guide = container.safeAreaLayoutGuide;
  [button.trailingAnchor constraintEqualToAnchor:guide.trailingAnchor
                                        constant:-kButtonMargin].active = YES;
  [button.bottomAnchor constraintEqualToAnchor:guide.bottomAnchor
                                      constant:-kButtonMargin].active = YES;

  [button addTarget:self action:@selector(onTap:)
   forControlEvents:UIControlEventTouchUpInside];
  self.button = button;
  self.button.hidden = YES;
}

- (void)onTap:(UIButton*)sender {
  [self presentFromAnchor:sender];
}

- (void)updateVisible:(BOOL)visible {
  if (!self.button) return;
  if (visible == !self.button.hidden) return;
  __weak VivaldiReaderModeFloatingUI* weakSelf = self;
  if (visible) {
    self.button.alpha = 0.0;
    self.button.hidden = NO;
    [UIView animateWithDuration:kFadeDuration
                     animations:^{ weakSelf.button.alpha = 1.0; }
    ];
  } else {
    [UIView animateWithDuration:kFadeDuration
                     animations:^{ weakSelf.button.alpha = 0.0; }
                     completion:^(BOOL finished) {
                       if (weakSelf) {
                         weakSelf.button.hidden = YES;
                         weakSelf.button.alpha = 1.0;
                       }
                     }
    ];
  }
}

- (UIViewController*)baseViewControllerForAnchor:(UIView*)anchor {
  UIView* containerView =
            self.containerProvider ? self.containerProvider() : nil;
  UIResponder* responder = containerView;
  while (responder && ![responder isKindOfClass:[UIViewController class]]) {
    responder = [responder nextResponder];
  }
  if ([responder isKindOfClass:[UIViewController class]]) {
    return (UIViewController*)responder;
  }
  if (anchor.window.rootViewController) {
    return anchor.window.rootViewController;
  }
  return nil;
}

@end
