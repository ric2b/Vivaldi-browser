// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#import "ios/panel/panel_view_controller.h"
#include <stdint.h>

#include "base/strings/utf_string_conversions.h"
#include "ios/chrome/browser/ui/history/history_coordinator.h"
#include "ios/chrome/browser/ui/history/history_table_view_controller.h"
#import "ios/chrome/browser/ui/table_view/table_view_navigation_controller.h"
#import "ios/chrome/browser/ui/table_view/table_view_presentation_controller.h"
#import "ios/chrome/browser/ui/table_view/table_view_presentation_controller_delegate.h"
#import "ios/chrome/browser/url_loading/url_loading_params.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/notes/note_home_view_controller.h"
#import "ios/panel/panel_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"
#import "vivaldi/mobile_common/grit/vivaldi_mobile_common_native_strings.h"

using l10n_util::GetNSString;

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PanelViewController (){
    NSArray *pages;
    UISegmentedControl* segmentedControl;
    NoteNavigationController* noteNavigationController;
    UINavigationController* historyController;
    UINavigationController* bookmarkController;
    UIView* pageSwitcherBackgroundView;
    NSLayoutConstraint* positionConstraint;
}

@property(nonatomic, strong) UIPageViewController* pageController;

- (void)segmentTapped:(UISegmentedControl*)sender;

@end

@implementation PanelViewController

- (instancetype)init {
    return [super initWithNibName:nil bundle:nil];
}

- (void)viewDidLayoutSubviews {
 [super viewDidLayoutSubviews];
 if ([self.pageController.viewControllers count] == 0) {
     return;
 }
 UINavigationController* navController =
     self.pageController.viewControllers[0];
 if (!navController) return;
 int position = navController.navigationBar.frame.size.height;
 // Adjust topAnchor if neccessary
 if (position != 0.0) {
     positionConstraint.active = NO;
     positionConstraint.constant = position;
     positionConstraint.active = YES;
 }
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

  UIView* topView = [[UIView alloc] init];
  pageSwitcherBackgroundView = topView;
  topView.frame = CGRectMake(0, 0, 0, panel_top_view_height);
  topView.backgroundColor = [UIColor colorNamed:kGroupedPrimaryBackgroundColor];
  topView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:topView];

  NSLayoutConstraint* centerHorizontallyConstraint2 =
      [NSLayoutConstraint
                    constraintWithItem: topView
                    attribute:NSLayoutAttributeCenterX
                    relatedBy:NSLayoutRelationEqual
                    toItem:self.view
                    attribute:NSLayoutAttributeCenterX
                    multiplier:1.0
                    constant:0];
  [topView.leadingAnchor constraintEqualToAnchor:
      self.view.leadingAnchor constant:0].active = YES;
  [self.view addConstraint:centerHorizontallyConstraint2];
  positionConstraint = [topView.topAnchor constraintEqualToAnchor:
                        self.view.topAnchor constant:panel_search_view_height];
  [topView.heightAnchor constraintEqualToConstant:panel_search_view_height]
        .active = YES;
  positionConstraint.active = YES;

  // Create and add segmentedControl
  segmentedControl = [[UISegmentedControl alloc]
                      initWithItems:@[
                l10n_util::GetNSString(IDS_IOS_TOOLS_MENU_BOOKMARKS),
                l10n_util::GetNSString(IDS_VIVALDI_TOOLS_MENU_NOTES),
                l10n_util::GetNSString(IDS_IOS_TOOLS_MENU_HISTORY)]];
  [segmentedControl addTarget:self action:@selector(segmentTapped:)
                 forControlEvents:UIControlEventValueChanged];
  self.segmentControl = segmentedControl;
  segmentedControl.translatesAutoresizingMaskIntoConstraints = NO;
  [topView addSubview:segmentedControl];
  NSLayoutConstraint* centerHorizontallyConstraint =
    [NSLayoutConstraint
                  constraintWithItem: segmentedControl
                  attribute:NSLayoutAttributeCenterX
                  relatedBy:NSLayoutRelationEqual
                  toItem:topView
                  attribute:NSLayoutAttributeCenterX
                  multiplier:1.0
                  constant:0];
  [segmentedControl.topAnchor constraintEqualToAnchor:
    topView.topAnchor constant:12].active = YES;
  [segmentedControl.leadingAnchor constraintEqualToAnchor:
    self.view.leadingAnchor constant:12].active = YES;
  [topView addConstraint:centerHorizontallyConstraint];
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

- (void)segmentTapped:(UISegmentedControl*)sender {
    if (!sender || sender.selectedSegmentIndex < 0
        || sender.selectedSegmentIndex > 2)
        return;
    UINavigationController* navController = nil;
    navController = self.pageController.viewControllers.firstObject;
    if (navController) {
      UIViewController* vc = [navController visibleViewController];
        if ([vc isKindOfClass:[UISearchController class]]) {
            [navController dismissViewControllerAnimated:YES completion:^{
                ((UISearchController*)vc).active = NO;
                int index = sender.selectedSegmentIndex;
                [self setIndexForControl:index];
            }];
            return;
        }
    }

    int index = sender.selectedSegmentIndex;
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

@end
