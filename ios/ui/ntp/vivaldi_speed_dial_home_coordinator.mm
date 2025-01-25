// Copyright 2024 Vivaldi Technologies. All rights reserved.

#import "ios/ui/ntp/vivaldi_speed_dial_home_coordinator.h"

#import "components/bookmarks/browser/bookmark_model.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_model_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"

using bookmarks::BookmarkModel;

@interface VivaldiSpeedDialHomeCoordinator () {
  // The browser where the settings are being displayed.
  Browser* _browser;
  // Bookmark model in which meditor and speed dial home controller depends
  BookmarkModel* _bookmarkModel;
  // Browser state in which meditor depends
  ChromeBrowserState* _browserState;
}

// View controller for the speed dial home/start page.
@property(nonatomic, strong) VivaldiSpeedDialBaseController* viewController;

// Navigation controller for the base view controller.
@property(nonatomic, strong) UINavigationController* navigationController;

// Mediator class for the speed dial home/start page.
@property(nonatomic, strong) VivaldiSpeedDialHomeMediator* mediator;

@end

@implementation VivaldiSpeedDialHomeCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser {
  self = [super initWithBaseViewController:viewController
                                   browser:browser];

  if (self) {
    _browser = browser;
    _browserState =
        _browser->GetBrowserState()->GetOriginalChromeBrowserState();
    _bookmarkModel =
        ios::BookmarkModelFactory::GetForBrowserState(_browserState);
  }

  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  VivaldiSpeedDialBaseController* controller =
      [[VivaldiSpeedDialBaseController alloc] initWithBrowser:_browser
                                                bookmarkModel:_bookmarkModel];
  self.viewController = controller;

  UINavigationController *navigationController =
      [[UINavigationController alloc]
          initWithRootViewController:controller];
  self.navigationController = navigationController;
  [navigationController.view layoutIfNeeded];

  self.mediator = [[VivaldiSpeedDialHomeMediator alloc]
                      initWithBrowserState:_browserState
                          bookmarkModel:_bookmarkModel];
  self.mediator.consumer = controller;
  [self.mediator startMediating];

  controller.delegate = self.mediator;
}

- (void)stop {
  [super stop];
  [self.viewController invalidate];
  self.viewController.delegate = nil;
  self.viewController = nil;
  self.navigationController = nil;

  self.mediator.consumer = nil;
  [self.mediator disconnect];

  _browser = nullptr;
  _bookmarkModel = nullptr;
  _browserState = nullptr;
}

@end
