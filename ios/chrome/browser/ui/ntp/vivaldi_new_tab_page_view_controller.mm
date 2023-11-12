// Copyright 2022 Vivaldi Technologies. All rights reserved.

#import "ios/chrome/browser/ui/ntp/vivaldi_new_tab_page_view_controller.h"

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/commands/popup_menu_commands.h"
#import "ios/chrome/browser/ui/ntp/top_menu/vivaldi_ntp_top_menu.h"
#import "ios/chrome/browser/ui/ntp/vivaldi_ntp_constants.h"
#import "ios/chrome/browser/ui/ntp/vivaldi_speed_dial_base_controller.h"
#import "ios/chrome/browser/ui/ntp/vivaldi_speed_dial_constants.h"
#import "ios/ui/helpers/vivaldi_uiview_layout_helper.h"
#import "ios/ui/helpers/vivaldi_uiviewcontroller_helper.h"
#import "ui/base/l10n/l10n_util.h"
#import "vivaldi/ios/grit/vivaldi_ios_native_strings.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface VivaldiNewTabPageViewController ()
                                    <VivaldiSpeedDialBaseControllerDelegate> {}
// View controlled to hold the speed dial collection base with top menu
@property(nonatomic, strong) VivaldiSpeedDialBaseController*
  speedDialViewController;
// The Browser in which bookmarks are presented
@property(nonatomic, assign) Browser* browser;

@end

@implementation VivaldiNewTabPageViewController
@synthesize speedDialViewController = _speedDialViewController;
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
}

-(void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  [self.speedDialViewController popPushedViewControllers];
}

#pragma mark - PRIVATE METHODS

/// Set up the UI Components
- (void)setUpUI {
  self.view.backgroundColor = [UIColor colorNamed:vNTPBackgroundColor];
  [self setupSpeedDialView];
}

/// Set up the speed dial view
-(void)setupSpeedDialView {
  // The container view to hold the speed dial view
  UIView* bodyContainerView = [UIView new];
  bodyContainerView.backgroundColor = [UIColor colorNamed:vNTPBackgroundColor];
  [self.view addSubview:bodyContainerView];

  [bodyContainerView anchorTop: self.view.safeTopAnchor
                       leading: self.view.leadingAnchor
                        bottom: self.view.bottomAnchor
                      trailing: self.view.trailingAnchor];

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

#pragma mark - VIVALDI SPEED DIAL BASE CONTROLLER DELEGATE

- (void)didTapSpeedDialWithItem:(VivaldiSpeedDialItem*)item
                captureSnapshot:(BOOL)captureSnapshot{
  if (self.delegate)
    [self.delegate didTapSpeedDial:item
                   captureSnapshot:captureSnapshot];
}

@end
