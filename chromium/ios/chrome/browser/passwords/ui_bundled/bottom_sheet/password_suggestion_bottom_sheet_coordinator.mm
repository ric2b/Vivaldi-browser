// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/ui_bundled/bottom_sheet/password_suggestion_bottom_sheet_coordinator.h"

#import <optional>

#import "components/keyed_service/core/service_access_type.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_account_password_store_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_profile_password_store_factory.h"
#import "ios/chrome/browser/passwords/model/password_controller_delegate.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/passwords/ui_bundled/bottom_sheet/password_suggestion_bottom_sheet_mediator.h"
#import "ios/chrome/browser/passwords/ui_bundled/bottom_sheet/password_suggestion_bottom_sheet_view_controller.h"
#import "ios/chrome/browser/passwords/ui_bundled/bottom_sheet/scoped_password_suggestion_bottom_sheet_reauth_module_override.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"
#import "ios/web/public/web_state.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

using PasswordSuggestionBottomSheetExitReason::kDismissal;
using PasswordSuggestionBottomSheetExitReason::kShowPasswordDetails;
using PasswordSuggestionBottomSheetExitReason::kShowPasswordManager;
using PasswordSuggestionBottomSheetExitReason::kUsePasswordSuggestion;

@interface PasswordSuggestionBottomSheetCoordinator () {
  // The password controller delegate used to open the password manager.
  id<PasswordControllerDelegate> _passwordControllerDelegate;

  // Currently in the process of dismissing the bottom sheet.
  bool _dismissing;
}

// This mediator is used to fetch data related to the bottom sheet.
@property(nonatomic, strong) PasswordSuggestionBottomSheetMediator* mediator;

// This view controller is used to display the bottom sheet.
@property(nonatomic, strong)
    PasswordSuggestionBottomSheetViewController* viewController;

// Module handling reauthentication before accessing sensitive data.
@property(nonatomic, strong) id<ReauthenticationProtocol> reauthModule;

@end

@implementation PasswordSuggestionBottomSheetCoordinator

- (instancetype)
    initWithBaseViewController:(UIViewController*)viewController
                       browser:(Browser*)browser
                        params:(const autofill::FormActivityParams&)params
                      delegate:(id<PasswordControllerDelegate>)delegate {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _passwordControllerDelegate = delegate;
    _dismissing = NO;

    WebStateList* webStateList = browser->GetWebStateList();
    const GURL& URL = webStateList->GetActiveWebState()->GetLastCommittedURL();
    self.viewController = [[PasswordSuggestionBottomSheetViewController alloc]
        initWithHandler:self
                    URL:URL];

    ChromeBrowserState* browserState =
        browser->GetBrowserState()->GetOriginalChromeBrowserState();

    auto profilePasswordStore =
        IOSChromeProfilePasswordStoreFactory::GetForBrowserState(
            browserState, ServiceAccessType::EXPLICIT_ACCESS);
    auto accountPasswordStore =
        IOSChromeAccountPasswordStoreFactory::GetForBrowserState(
            browserState, ServiceAccessType::EXPLICIT_ACCESS);

    self.reauthModule =
        ScopedPasswordSuggestionBottomSheetReauthModuleOverride::Get();
    if (!self.reauthModule) {
      self.reauthModule = [[ReauthenticationModule alloc] init];
    }
    self.mediator = [[PasswordSuggestionBottomSheetMediator alloc]
          initWithWebStateList:webStateList
                 faviconLoader:IOSChromeFaviconLoaderFactory::
                                   GetForBrowserState(browserState)
                   prefService:browserState->GetPrefs()
                        params:params
                  reauthModule:_reauthModule
                           URL:URL
          profilePasswordStore:profilePasswordStore
          accountPasswordStore:accountPasswordStore
        sharedURLLoaderFactory:browserState->GetSharedURLLoaderFactory()
             engagementTracker:feature_engagement::TrackerFactory::
                                   GetForBrowserState(
                                       self.browser->GetBrowserState())];
    self.viewController.delegate = self.mediator;
    self.mediator.consumer = self.viewController;
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  // If the bottom sheet has no suggestion to show, do not show the bottom
  // sheet. Instead, re-focus the field which triggered the bottom sheet and
  // disable it.
  if (![self.mediator hasSuggestions]) {
    [self.mediator dismiss];
    return;
  }

  self.viewController.parentViewControllerHeight =
      self.baseViewController.view.frame.size.height;
  __weak __typeof(self) weakSelf = self;
  [self.baseViewController presentViewController:self.viewController
                                        animated:YES
                                      completion:^{
                                        [weakSelf setInitialVoiceOverFocus];
                                      }];
}

- (void)stop {
  [super stop];
  [_mediator disconnect];
  _mediator.consumer = nil;
  _mediator = nil;
  _viewController.delegate = nil;
  _viewController = nil;
}

#pragma mark - PasswordSuggestionBottomSheetHandler

- (void)displayPasswordManager {
  _dismissing = YES;
  [self.mediator logExitReason:kShowPasswordManager];

  __weak __typeof(self) weakSelf = self;
  [self.baseViewController.presentedViewController
      dismissViewControllerAnimated:NO
                         completion:^{
                           [weakSelf displaySavedPasswordList];
                           [weakSelf.browserCoordinatorCommandsHandler
                                   dismissPasswordSuggestions];
                         }];
}

- (void)displayPasswordDetailsForFormSuggestion:
    (FormSuggestion*)formSuggestion {
  _dismissing = YES;
  [self.mediator logExitReason:kShowPasswordDetails];
  std::optional<password_manager::CredentialUIEntry> credential =
      [self.mediator getCredentialForFormSuggestion:formSuggestion];

  __weak __typeof(self) weakSelf = self;
  [self.baseViewController.presentedViewController
      dismissViewControllerAnimated:NO
                         completion:^{
                           // TODO(crbug.com/40896839): Add metric for when the
                           // credential is nil.
                           if (credential.has_value()) {
                             [weakSelf
                                 showPasswordDetailsForCredential:credential
                                                                      .value()];
                           }
                           [weakSelf.browserCoordinatorCommandsHandler
                                   dismissPasswordSuggestions];
                         }];
}

- (void)primaryButtonTapped:(FormSuggestion*)formSuggestion {
  _dismissing = YES;
  [self.mediator logExitReason:kUsePasswordSuggestion];
  __weak __typeof(self) weakSelf = self;
  ProceduralBlock completion = ^{
    [weakSelf.browserCoordinatorCommandsHandler dismissPasswordSuggestions];
  };
  [self.viewController
      dismissViewControllerAnimated:NO
                         completion:^{
                           [weakSelf.mediator didSelectSuggestion:formSuggestion
                                                       completion:completion];
                         }];
}

- (void)secondaryButtonTapped {
  // "Use Keyboard" button, which dismisses the bottom sheet.
  [self.viewController dismissViewControllerAnimated:YES completion:NULL];
}

- (void)viewDidDisappear {
  if (_dismissing) {
    return;
  }

  [self.mediator logExitReason:kDismissal];
  [self.mediator dismiss];
  [self.mediator disconnect];
  [_browserCoordinatorCommandsHandler dismissPasswordSuggestions];
}

#pragma mark - Private

- (void)setInitialVoiceOverFocus {
  UIAccessibilityPostNotification(UIAccessibilityScreenChangedNotification,
                                  self.viewController.aboveTitleView);
}

- (void)displaySavedPasswordList {
  [_passwordControllerDelegate displaySavedPasswordList];
}

- (void)showPasswordDetailsForCredential:
    (password_manager::CredentialUIEntry)credential {
  [_passwordControllerDelegate showPasswordDetailsForCredential:credential];
}

@end
