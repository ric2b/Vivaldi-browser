// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#import "ios/panel/sidebar_panel_view_controller.h"
#include <stdint.h>

#import "base/strings/utf_string_conversions.h"
#import "ios/chrome/browser/ui/history/history_coordinator.h"
#import "ios/chrome/browser/ui/history/history_table_view_controller.h"
#import "ios/chrome/browser/ui/table_view/table_view_navigation_controller.h"
#import "ios/chrome/browser/ui/table_view/table_view_presentation_controller.h"
#import "ios/chrome/browser/ui/table_view/table_view_presentation_controller_delegate.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_constants.h"
#import "ios/chrome/browser/url_loading/url_loading_params.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/notes/note_home_view_controller.h"
#import "ios/panel/panel_button_view.h"
#import "ios/panel/panel_constants.h"
#import "ios/panel/panel_transitioning_delegate.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "vivaldi/mobile_common/grit/vivaldi_mobile_common_native_strings.h"
#import "ios/ui/helpers/vivaldi_uiview_layout_helper.h"

using l10n_util::GetNSString;

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

UIEdgeInsets buttonViewPadding = UIEdgeInsetsMake(kAdaptiveToolbarMargin,0,0,0);

@interface SidebarPanelViewController ()
        <PanelButtonViewDelegate> {
    NoteNavigationController* noteNavigationController;
    UINavigationController* historyController;
    UINavigationController* bookmarkController;
}

@property(nonatomic, strong) UIPageViewController* pageController;

- (void)panelButtonClicked:(NSInteger)index;

@end

@implementation SidebarPanelViewController

- (instancetype)init {
    return [super initWithNibName:nil bundle:nil];
}

- (void)viewDidLoad {
  [super viewDidLoad];
  // Setup page controller and add to view
  self.pageController = [[UIPageViewController alloc]
      initWithTransitionStyle:UIPageViewControllerTransitionStyleScroll
        navigationOrientation:UIPageViewControllerNavigationOrientationVertical
                    options:nil];
  self.pageController.delegate = self;
  [self addChildViewController:self.pageController];
  [self.view addSubview:self.pageController.view];
  [self.pageController didMoveToParentViewController:self];
  CGFloat sidebarWidth = panel_icon_size + kAdaptiveToolbarMargin;
  [self.pageController.view fillSuperviewWithPadding:
    UIEdgeInsetsMake(0, sidebarWidth, 0, 0)];

  PanelButtonView* panelButtonView =
    [[PanelButtonView alloc] initWithFrame:CGRectZero];
  panelButtonView.delegate = self;
  [self.view addSubview:panelButtonView];
  CGSize buttonViewSize = CGSizeMake(sidebarWidth, 0);
  [panelButtonView anchorTop:self.view.topAnchor
                     leading:self.view.leadingAnchor
                      bottom:self.view.bottomAnchor
                    trailing:nil
                    padding:buttonViewPadding
                       size:buttonViewSize];
}

/*
  Setup controllers
 */
- (void)setupControllers:(NoteNavigationController*)nvc
  withBookmarkController:(UINavigationController*)bvc
    andHistoryController:(UINavigationController*)hc {
    noteNavigationController = nvc;
    bookmarkController = bvc;
    historyController = hc;
    nvc.navigationBar.prefersLargeTitles = NO;
    bvc.navigationBar.prefersLargeTitles = NO;
    hc.navigationBar.prefersLargeTitles = NO;
}

- (void)setIndexForControl:(int)index {
    UIViewController* uv = nil;
    switch (index) {
      case PanelPage::BookmarksPage:
          uv = bookmarkController;
            break;
      case PanelPage::NotesPage:
          uv = noteNavigationController;
            break;
      case PanelPage::HistoryPage:
          uv = historyController;
            break;
    }
    [self.pageController setViewControllers:@[uv]
                direction:UIPageViewControllerNavigationDirectionForward
                         animated:NO
                        completion:nil];
}


- (void)panelButtonClicked:(NSInteger)index {
    if (index < 0 || index > 2)
        return;
    UINavigationController* navController = nil;
    navController = self.pageController.viewControllers.firstObject;
    if (navController) {
      UIViewController* vc = [navController visibleViewController];
        if ([vc isKindOfClass:[UISearchController class]]) {
            [navController dismissViewControllerAnimated:YES completion:^{
                ((UISearchController*)vc).active = NO;
                [self setIndexForControl:index];
            }];
            return;
        }
    }
    [self setIndexForControl:index];
}

- (void)panelDismissed {
    [self.pageController.view removeFromSuperview];
    [self.pageController removeFromParentViewController];
    self.pageController = nil;
    bookmarkController = nil;
    noteNavigationController = nil;
    historyController = nil;
}

#pragma mark - PANEL BUTTON DELEGATE
- (void)didSelectIndex:(NSInteger)index {
    [self panelButtonClicked:index];
}

@end
