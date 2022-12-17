// Copyright 2022 Vivaldi Technologies. All rights reserved.

#import "ios/chrome/browser/ui/ntp/vivaldi_new_tab_page_view_controller.h"

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/ntp/top_menu/vivaldi_ntp_top_menu.h"
#import "ios/chrome/browser/ui/ntp/vivaldi_ntp_constants.h"
#import "ios/chrome/browser/ui/ntp/vivaldi_speed_dial_base_controller.h"
#import "ios/chrome/browser/ui/ntp/vivaldi_speed_dial_shared_state.h"
#import "ios/ui/helpers/vivaldi_uiview_layout_helper.h"
#import "ios/ui/helpers/vivaldi_uiviewcontroller_helper.h"
#import "ui/base/l10n/l10n_util.h"
#import "vivaldi/ios/grit/vivaldi_ios_native_strings.h"
#include "ui/base/device_form_factor.h"

using ui::GetDeviceFormFactor;
using ui::DEVICE_FORM_FACTOR_TABLET;

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Returns an UIImageView for the given imgageName.
UIImageView* ImageViewForImageNamed(NSString* imageName) {
  return [[UIImageView alloc]
      initWithImage:
          [[UIImage imageNamed:imageName]
              imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate]];
}

}  // namespace

@interface VivaldiNewTabPageViewController ()
                                    <VivaldiSpeedDialBaseControllerDelegate> {}
// The view contains the fake search bar
@property(nonatomic, weak) UIView* headerView;
// Search icon imageview
@property(nonatomic, weak) UIImageView* searchIconView;
// Search label
@property(nonatomic, weak) UILabel* searchLabel;
// View controlled to hold the speed dial collection base with top menu
@property(nonatomic, strong) VivaldiSpeedDialBaseController*
  speedDialViewController;
// The Browser in which bookmarks are presented
@property(nonatomic, assign) Browser* browser;
// Height constraint of the header view that contains the search bar
@property (assign,nonatomic) NSLayoutConstraint* headerHeight;

@end

@implementation VivaldiNewTabPageViewController
@synthesize headerView = _headerView;
@synthesize speedDialViewController = _speedDialViewController;
@synthesize searchIconView = _searchIconView;
@synthesize searchLabel = _searchLabel;
@synthesize browser = _browser;

#pragma mark - Initializer
- (instancetype)initWithBrowser:(Browser*)browser {
  DCHECK(browser);
  self = [super init];
  if (self) {
    _browser = browser;
  }
  return self;
}

- (void)dealloc {
  _speedDialViewController = nullptr;
  _speedDialViewController.delegate = nil;
}

#pragma mark - VIEW CONTROLLER LIFECYCLE
- (void)viewDidLoad {
  [super viewDidLoad];
  [self setUpUI];
  [self resetSelectedMenuIndex];
}

-(void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  [self.speedDialViewController popPushedViewControllers];
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if ((self.traitCollection.verticalSizeClass !=
          previousTraitCollection.verticalSizeClass) ||
      (self.traitCollection.horizontalSizeClass !=
          previousTraitCollection.horizontalSizeClass)) {
    [self handleDeviceOrientationChange];
  }
}

#pragma mark - ACTIONS
- (void)handleSearchBarTap:(UITapGestureRecognizer *)recognizer {
  if (self.delegate)
    [self.delegate didTapSearchBar];
}

#pragma mark - PRIVATE METHODS

/// Set up the UI Components
- (void)setUpUI {
  self.view.backgroundColor = [UIColor colorNamed:vNTPBackgroundColor];
  [self setUpHeaderView];
  [self setupSpeedDialView];
}

/// Set up the header view
- (void)setUpHeaderView {
  // Header base
  UIView *headerView = [UIView new];
  _headerView = headerView;
  self.headerView.backgroundColor = [UIColor colorNamed:vNTPBackgroundColor];
  [self.view addSubview:self.headerView];

  [self.headerView anchorTop: self.view.safeTopAnchor
                     leading: self.view.safeLeftAnchor
                      bottom: nil
                    trailing: self.view.safeRightAnchor];
  self.headerHeight =
    [self.headerView.heightAnchor
      constraintEqualToConstant:vNTPSearchBarHeight - vNTPSearchBarHeightOffset];
  [self.headerHeight setActive:YES];

  // Fake search view
  UIView *fakeSearchBarView = [UIView new];
  fakeSearchBarView.backgroundColor =
      [UIColor colorNamed:vSearchbarBackgroundColor];
  fakeSearchBarView.layer.cornerRadius = vNTPSearchBarCornerRadius;
  [self.headerView addSubview:fakeSearchBarView];

  [fakeSearchBarView anchorTop: self.headerView.topAnchor
                       leading: self.headerView.leadingAnchor
                        bottom: self.headerView.bottomAnchor
                      trailing: self.headerView.trailingAnchor
                       padding: vNTPSearchBarPadding];

  // Search Label
  UILabel* label = [[UILabel alloc] init];
  _searchLabel = label;
  NSString* searchTitleString =
    l10n_util::GetNSString(IDS_IOS_SEARCH_OR_TYPE_WEB_ADDRESS);
  label.text = searchTitleString;
  label.accessibilityLabel = searchTitleString;
  label.textColor = [UIColor colorNamed:vSearchbarTextColor];
  label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  label.adjustsFontForContentSizeCategory = YES;
  label.numberOfLines = 0;
  label.textAlignment = NSTextAlignmentCenter;

  [fakeSearchBarView addSubview:_searchLabel];
  [_searchLabel centerInSuperview];

  // Search icon
  UIImageView* imageView = ImageViewForImageNamed(vNTPSearchIcon);
  _searchIconView = imageView;
  imageView.tintColor = [UIColor colorNamed:vSearchbarTextColor];
  [fakeSearchBarView addSubview:_searchIconView];

  [_searchIconView anchorTop: nil
                     leading: nil
                      bottom: nil
                    trailing: _searchLabel.leadingAnchor
                     padding: vNTPSearchBarSearchIconPadding
                        size: vNTPSearchBarSearchIconSize];
  [_searchIconView centerYInSuperview];

  // A a view over the search bar for tap gesture
  UIView *fakeSearchBarTouchView = [UIView new];
  fakeSearchBarTouchView.backgroundColor = [UIColor clearColor];
  [fakeSearchBarView addSubview:fakeSearchBarTouchView];
  [fakeSearchBarTouchView matchToView:fakeSearchBarView];

  UITapGestureRecognizer *searchBarTapGesture =
    [[UITapGestureRecognizer alloc] initWithTarget:self
                                    action:@selector(handleSearchBarTap:)];
  [fakeSearchBarTouchView addGestureRecognizer:searchBarTapGesture];
}

/// Set up the speed dial view
-(void)setupSpeedDialView {
  // The container view to hold the speed dial view
  UIView* bodyContainerView = [UIView new];
  bodyContainerView.backgroundColor = [UIColor colorNamed:vNTPBackgroundColor];
  [self.view addSubview:bodyContainerView];

  [bodyContainerView anchorTop: nil
                       leading: self.view.safeLeftAnchor
                        bottom: self.view.bottomAnchor
                      trailing: self.view.safeRightAnchor];

  if (GetDeviceFormFactor() == DEVICE_FORM_FACTOR_TABLET) {
    [[bodyContainerView.topAnchor
      constraintEqualToAnchor: self.view.safeTopAnchor] setActive:YES];
    [self.headerView setHidden:YES];
  } else {
    [[bodyContainerView.topAnchor
      constraintEqualToAnchor:self.headerView.bottomAnchor
        constant:vSpeedDialContainerTopPadding]
                    setActive:YES];
    [self handleHeaderViewVisiblity];
  }

  // Speed dial view controller
  _speedDialViewController =
    [[VivaldiSpeedDialBaseController alloc] initWithBrowser:self.browser];
  UINavigationController *newVC =
      [[UINavigationController alloc]
        initWithRootViewController:_speedDialViewController];
  _speedDialViewController.delegate = self;

  [newVC
      willMoveToParentViewController:self];
  [self addChildViewController:newVC];
  [bodyContainerView addSubview:newVC.view];
  [newVC.view fillSuperview];
  [newVC
      didMoveToParentViewController:self];
}

/// Device orientation change handler
- (void)handleDeviceOrientationChange {
  if (GetDeviceFormFactor() == DEVICE_FORM_FACTOR_TABLET ||
      !self.hasValidOrientation)
    return;
  [self handleHeaderViewVisiblity];
}

/// Control header view visiblity
- (void)handleHeaderViewVisiblity {
  if (!self.isDevicePortrait) {
    [self.headerView setHidden:YES];
    self.headerHeight.constant = 0;
  } else {
    [self.headerView setHidden:NO];
    self.headerHeight.constant = vNTPSearchBarHeight - vNTPSearchBarHeightOffset;
  }
}

/// Reset the selected menu item to 'default'. Opening new tab page when there's
/// no tab available from the past should open with 'Speed Dial' item selected.
- (void)resetSelectedMenuIndex {
  VivaldiSpeedDialSharedState *sharedState =
      [VivaldiSpeedDialSharedState manager];
  sharedState.selectedIndex = 0;
}

#pragma mark - VIVALDI SPEED DIAL BASE CONTROLLER DELEGATE

- (void)didTapSpeedDialWithItem:(VivaldiSpeedDialItem *)item {
  if (self.delegate)
    [self.delegate didTapSpeedDial:item];
}

@end
