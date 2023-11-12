// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#import "ios/panel/sidebar_panel_presentation_controller.h"

#import "base/apple/foundation_util.h"
#import "ios/panel/panel_constants.h"
#import "ios/panel/slide_in_animator.h"
#import "ios/panel/slide_out_animator.h"
#import "ios/ui/helpers/vivaldi_uiview_layout_helper.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SidebarPanelPresentationController() {
}

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
   frame.size.height = [self containerView].frame.size.height
                            - panel_sidebar_top;
   frame.origin = CGPointMake(0, panel_sidebar_top);
   return frame;
}

- (void)presentationTransitionWillBegin {
    [[self containerView] addSubview:self.backgroundView];
    [self.backgroundView addSubview:[self.presentedViewController view]];
    [self.backgroundView fillSuperview];
    [self.dimmingView fillSuperviewWithPadding:UIEdgeInsetsMake(
                                            panel_sidebar_top, 0, 0, 0)];
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
                      parentSize.height - panel_sidebar_top);
}

- (void)presentationTransitionDidEnd {}

@end
