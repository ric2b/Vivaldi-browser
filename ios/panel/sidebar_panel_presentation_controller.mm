// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#import "ios/panel/sidebar_panel_presentation_controller.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_constants.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_utils.h"
#import "ios/chrome/browser/tabs/ui_bundled/tab_strip_constants.h"
#import "ios/panel/panel_constants.h"
#import "ios/panel/slide_in_animator.h"
#import "ios/panel/slide_out_animator.h"
#import "ios/ui/helpers/vivaldi_uiview_layout_helper.h"

@interface SidebarPanelPresentationController()

@property(nonatomic, strong) UIView* backgroundView;
@property(nonatomic, strong) UIView* dimmingView;
@property(nonatomic, strong) UIGestureRecognizer* recognizer;

@end

@implementation SidebarPanelPresentationController

- (instancetype)initWithPresentedViewController:
            (UIViewController*)presentedViewController
                presentingViewController:
            (UIViewController*)presentingViewController {
    self = [super initWithPresentedViewController:presentedViewController
            presentingViewController:presentingViewController];
    [self setupDimmingView];
    return self;
}

- (void)setupDimmingView {
    self.backgroundView = [[UIView alloc] init];
    self.backgroundView.translatesAutoresizingMaskIntoConstraints = NO;
    self.backgroundView.alpha = 0.0;

    self.dimmingView = [[UIView alloc] init];
    self.dimmingView.translatesAutoresizingMaskIntoConstraints = NO;
    self.dimmingView.backgroundColor = UIColor.blackColor;
    self.dimmingView.alpha = 1.0;
    [self.backgroundView addSubview:self.dimmingView];
    self.recognizer = [[UITapGestureRecognizer alloc] initWithTarget:self
                                action:@selector(handleTap:)];
    [self.backgroundView addGestureRecognizer:self.recognizer];
}

- (void)handleTap:(UIGestureRecognizer*)recognizer {
    [self.presentingViewController dismissViewControllerAnimated:YES
                                                    completion:nil];
}

-(CGRect)frameOfPresentedViewInContainerView {
  CGRect frame = CGRectZero;
  frame.size = [self sizeForChildContentContainer:self
      withParentContainerSize:self.containerView.bounds.size];
  frame.size.width = panel_sidebar_width + panel_icon_size;
  frame.size.height =
      [self presentedViewHeightFromParentHeight:
          [self containerView].frame.size.height];
  frame.origin = CGPointMake(0, [self topPadding]);
  return frame;
}

- (void)presentationTransitionWillBegin {
  [[self containerView] addSubview:self.backgroundView];
  [self.backgroundView addSubview:[self.presentedViewController view]];
  [self.backgroundView fillSuperview];
  [self.dimmingView
      fillSuperviewWithPadding:UIEdgeInsetsMake(
          [self topPadding], 0, [self bottomPadding], 0)];
  self.dimmingView.backgroundColor = UIColor.blackColor;
  [self.backgroundView.topAnchor constraintEqualToAnchor:
   self.presentedViewController.view.topAnchor].active = YES;
  [super presentationTransitionWillBegin];
  [self.presentedViewController.transitionCoordinator
   animateAlongsideTransition:
     ^(id<UIViewControllerTransitionCoordinatorContext> _Nonnull context) {
    self.backgroundView.alpha = 0.5;
  } completion:^(id<UIViewControllerTransitionCoordinatorContext>
                 _Nonnull context) {
  }];
}

- (void)dismissalTransitionWillBegin {
    [self.presentedViewController.transitionCoordinator
     animateAlongsideTransition:^(
        id<UIViewControllerTransitionCoordinatorContext> _Nonnull context) {
            self.backgroundView.alpha = 0;
        } completion:^(id<UIViewControllerTransitionCoordinatorContext>
                       _Nonnull context) {
     }];
}

- (void)containerViewWillLayoutSubviews {
    self.presentedView.frame = [self frameOfPresentedViewInContainerView];
}

- (CGSize)sizeForChildContentContainer:(id<UIContentContainer>)container
                 withParentContentSize:(CGSize)parentSize {
  return CGSizeMake(panel_sidebar_width + panel_icon_size,
      [self presentedViewHeightFromParentHeight:parentSize.height]);
}

- (void)presentationTransitionDidEnd {}

- (CGFloat)presentedViewHeightFromParentHeight:(CGFloat)parentHeight {
  return parentHeight - [self topPadding] - [self bottomPadding];
}

// Calculate top padding from location bar height, safe top area insets,
// tab bar height, and toolbar type.
- (CGFloat)topPadding {
  if (self.toolbarType == ToolbarType::kPrimary) {
    return [self toolbarHeight] +
        kTabStripHeight + kTopToolbarUnsplitMargin + self.safeAreaInsets.top;
  } else {
    return self.safeAreaInsets.top;
  }
}

// Calculate bottom padding from location bar height, safe bottom area insets,
// tab bar height, and toolbar type.
- (CGFloat)bottomPadding {
  if (self.toolbarType == ToolbarType::kPrimary) {
    return 0;
  } else {
    return [self toolbarHeight] + kSecondaryToolbarWithoutOmniboxHeight +
        self.safeAreaInsets.bottom;
  }
}

- (CGFloat)toolbarHeight {
  return ToolbarExpandedHeight(
      self.traitCollection.preferredContentSizeCategory);
}

- (UIEdgeInsets)safeAreaInsets {
  if (self.containerView) {
    UIEdgeInsets safeAreaInsets = self.containerView.safeAreaInsets;
    return safeAreaInsets;
  }
  return UIEdgeInsetsZero;
}

@end
