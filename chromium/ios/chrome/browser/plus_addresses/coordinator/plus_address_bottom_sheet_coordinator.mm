// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/plus_addresses/coordinator/plus_address_bottom_sheet_coordinator.h"

#import "base/strings/sys_string_conversions.h"
#import "components/plus_addresses/plus_address_service.h"
#import "components/plus_addresses/plus_address_types.h"
#import "components/plus_addresses/settings/plus_address_setting_service.h"
#import "ios/chrome/browser/autofill/model/bottom_sheet/autofill_bottom_sheet_tab_helper.h"
#import "ios/chrome/browser/plus_addresses/coordinator/plus_address_bottom_sheet_mediator.h"
#import "ios/chrome/browser/plus_addresses/model/plus_address_service_factory.h"
#import "ios/chrome/browser/plus_addresses/model/plus_address_setting_service_factory.h"
#import "ios/chrome/browser/plus_addresses/ui/plus_address_bottom_sheet_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"

namespace {
constexpr CGFloat kHalfSheetCornerRadius = 20;
}  // namespace

@implementation PlusAddressBottomSheetCoordinator {
  // The view controller responsible for display of the bottom sheet.
  PlusAddressBottomSheetViewController* _viewController;
  // A mediator that hides data operations from the view controller.
  PlusAddressBottomSheetMediator* _mediator;
}

#pragma mark - ChromeCoordinator

- (void)start {
  ChromeBrowserState* browserState =
      self.browser->GetBrowserState()->GetOriginalChromeBrowserState();
  plus_addresses::PlusAddressService* plusAddressService =
      PlusAddressServiceFactory::GetForProfile(browserState);
  plus_addresses::PlusAddressSettingService* plusAddressSettingService =
      PlusAddressSettingServiceFactory::GetForProfile(browserState);
  web::WebState* activeWebState =
      self.browser->GetWebStateList()->GetActiveWebState();
  // TODO(crbug.com/40276862): Move this to the mediator to reduce model
  // dependencies in this class.
  AutofillBottomSheetTabHelper* bottomSheetTabHelper =
      AutofillBottomSheetTabHelper::FromWebState(activeWebState);
  _mediator = [[PlusAddressBottomSheetMediator alloc]
      initWithPlusAddressService:plusAddressService
       plusAddressSettingService:plusAddressSettingService
                       activeUrl:activeWebState->GetLastCommittedURL()
                autofillCallback:bottomSheetTabHelper
                                     ->GetPendingPlusAddressFillCallback()
                       urlLoader:UrlLoadingBrowserAgent::FromBrowser(
                                     self.browser)
                       incognito:self.browser->GetBrowserState()
                                     ->IsOffTheRecord()];
  _viewController = [[PlusAddressBottomSheetViewController alloc]
                    initWithDelegate:_mediator
      withBrowserCoordinatorCommands:HandlerForProtocol(
                                         self.browser->GetCommandDispatcher(),
                                         BrowserCoordinatorCommands)];
  // Indicate a preference for half sheet detents, and other styling concerns.
  _viewController.modalPresentationStyle = UIModalPresentationPageSheet;
  UISheetPresentationController* presentationController =
      _viewController.sheetPresentationController;
  presentationController.prefersEdgeAttachedInCompactHeight = YES;
  presentationController.detents = @[
    UISheetPresentationControllerDetent.mediumDetent,
    UISheetPresentationControllerDetent.largeDetent
  ];
  presentationController.preferredCornerRadius = kHalfSheetCornerRadius;

  _mediator.consumer = _viewController;

  [self.baseViewController presentViewController:_viewController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [super stop];
  [_viewController.presentingViewController dismissViewControllerAnimated:YES
                                                               completion:nil];
  _viewController = nil;
  _mediator = nil;
}

@end
