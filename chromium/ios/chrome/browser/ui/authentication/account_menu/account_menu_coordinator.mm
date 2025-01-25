// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/account_menu/account_menu_coordinator.h"

#import "base/check.h"
#import "base/strings/sys_string_conversions.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_service_utils.h"
#import "components/sync/service/sync_user_settings.h"
#import "ios/chrome/browser/push_notification/model/push_notification_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/shared/ui/util/snackbar_util.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/system_identity_manager.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/ui/authentication/account_menu/account_menu_constants.h"
#import "ios/chrome/browser/ui/authentication/account_menu/account_menu_coordinator_delegate.h"
#import "ios/chrome/browser/ui/authentication/account_menu/account_menu_mediator.h"
#import "ios/chrome/browser/ui/authentication/account_menu/account_menu_mediator_delegate.h"
#import "ios/chrome/browser/ui/authentication/account_menu/account_menu_view_controller.h"
#import "ios/chrome/browser/ui/authentication/account_menu/account_menu_view_controller_presentation_delegate.h"
#import "ios/chrome/browser/ui/authentication/authentication_flow.h"
#import "ios/chrome/browser/ui/authentication/signout_action_sheet/signout_action_sheet_coordinator.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_accounts/accounts_coordinator.h"
#import "ios/chrome/browser/ui/settings/settings_root_view_controlling.h"
#import "ios/chrome/browser/ui/settings/sync/sync_encryption_passphrase_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/sync/sync_encryption_table_view_controller.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@interface AccountMenuCoordinator () <
    AccountMenuMediatorDelegate,
    AccountMenuViewControllerPresentationDelegate,
    SignoutActionSheetCoordinatorDelegate,
    UIAdaptivePresentationControllerDelegate,
    UINavigationControllerDelegate>
@end

@implementation AccountMenuCoordinator {
  AccountMenuViewController* _viewController;
  UINavigationController* _navigationController;
  AuthenticationService* _authenticationService;
  // Dismiss callback for account details view.
  SystemIdentityManager::DismissViewCallback
      _accountDetailsControllerDismissCallback;
  // The coordinators for the "Edit account list"
  AccountsCoordinator* _accountsCoordinator;
  AccountMenuMediator* _mediator;
  // The coordinator for the action sheet to sign out.
  SignoutActionSheetCoordinator* _signoutActionSheetCoordinator;
  raw_ptr<syncer::SyncService> _syncService;

  // ApplicationCommands handler.
  id<ApplicationCommands> _applicationHandler;
}

- (void)dealloc {
  CHECK(!_viewController);
}

- (void)start {
  [super start];

  ChromeBrowserState* browserState = self.browser->GetBrowserState();
  _syncService = SyncServiceFactory::GetForBrowserState(browserState);
  _authenticationService =
      AuthenticationServiceFactory::GetForBrowserState(browserState);
  ChromeAccountManagerService* accountManagerService =
      ChromeAccountManagerServiceFactory::GetForBrowserState(browserState);
  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForBrowserState(browserState);
  _applicationHandler = HandlerForProtocol(self.browser->GetCommandDispatcher(),
                                           ApplicationCommands);

  _viewController = [[AccountMenuViewController alloc]
      initWithStyle:ChromeTableViewStyle()];
  _viewController.delegate = self;

  _navigationController = [[UINavigationController alloc]
      initWithRootViewController:_viewController];
  _navigationController.delegate = self;

  _navigationController.modalPresentationStyle = UIModalPresentationPopover;
  _navigationController.popoverPresentationController.sourceView =
      self.anchorView;
  _navigationController.popoverPresentationController.permittedArrowDirections =
      UIPopoverArrowDirectionUp;
  _navigationController.presentationController.delegate = self;

  _mediator =
      [[AccountMenuMediator alloc] initWithSyncService:_syncService
                                 accountManagerService:accountManagerService
                                           authService:_authenticationService
                                       identityManager:identityManager];
  _mediator.delegate = self;
  _mediator.consumer = _viewController;
  _viewController.mutator = _mediator;
  _viewController.dataSource = _mediator;
  [_viewController setUpBottomSheetPresentationController];
  [self.baseViewController presentViewController:_navigationController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  // TODO(crbug.com/336719423): Change condition to CHECK(_viewController). But
  // first inform the parent coordinator at didTapClose that this view was
  // dismissed.
  if (!_viewController) {
    return;
  }
  if (!_accountDetailsControllerDismissCallback.is_null()) {
    std::move(_accountDetailsControllerDismissCallback).Run(/*animated=*/false);
  }
  [self stopAccountsCoordinator];
  [_navigationController dismissViewControllerAnimated:YES completion:nil];
  _authenticationService = nil;
  _navigationController.delegate = nil;
  _navigationController = nil;
  _viewController.dataSource = nil;
  _viewController.delegate = nil;
  _viewController.mutator = nil;
  _viewController = nil;
  [_mediator disconnect];
  _mediator.consumer = nil;
  _mediator.delegate = nil;
  _mediator = nil;
  _applicationHandler = nil;
  _syncService = nullptr;
  _authenticationService = nullptr;
  [self stopSignoutActionSheetCoordinator];
  [self stopAccountsCoordinator];
  [super stop];
}

#pragma mark - AccountMenuViewControllerPresentationDelegate

- (void)viewControllerWantsToBeClosed:
    (AccountMenuViewController*)viewController {
  CHECK_EQ(_viewController, viewController);
  [self.delegate acountMenuCoordinatorShouldStop:self];
}

- (void)didTapManageYourGoogleAccount {
  __weak __typeof(self) weakSelf = self;
  _accountDetailsControllerDismissCallback =
      GetApplicationContext()
          ->GetSystemIdentityManager()
          ->PresentAccountDetailsController(
              _authenticationService->GetPrimaryIdentity(
                  signin::ConsentLevel::kSignin),
              _viewController,
              /*animated=*/YES,
              base::BindOnce(
                  [](__typeof(self) strongSelf) {
                    [strongSelf resetAccountDetailsControllerDismissCallback];
                  },
                  weakSelf));
}

- (void)didTapEditAccountList {
  _accountsCoordinator = [[AccountsCoordinator alloc]
      initWithBaseViewController:_navigationController
                         browser:self.browser
       closeSettingsOnAddAccount:NO];
  _accountsCoordinator.signoutDismissalByParentCoordinator = YES;
  [_accountsCoordinator start];
}

- (void)signOutFromTargetRect:(CGRect)targetRect {
  if (_mediator.signOutFlowInProgress ||
      _mediator.addAccountOperationInProgress) {
    return;
  }
  if (!_authenticationService->HasPrimaryIdentity(
          signin::ConsentLevel::kSignin)) {
    // This could happen in very rare cases, if the account somehow got removed
    // after the accounts menu was created.
    return;
  }
  _signoutActionSheetCoordinator = [[SignoutActionSheetCoordinator alloc]
      initWithBaseViewController:_viewController
                         browser:self.browser
                            rect:targetRect
                            view:_viewController.view
                      withSource:signin_metrics::ProfileSignout::
                                     kUserClickedSignoutInAccountMenu];
  _signoutActionSheetCoordinator.delegate = self;
  __weak __typeof(self) weakSelf = self;
  _signoutActionSheetCoordinator.completion = ^(BOOL success) {
    [weakSelf stopSignoutActionSheetCoordinator];
    if (success) {
      [weakSelf.delegate acountMenuCoordinatorShouldStop:weakSelf];
    }
  };
  [_signoutActionSheetCoordinator start];
}

- (void)didTapAddAccount {
  if (_mediator.signOutFlowInProgress ||
      _mediator.addAccountOperationInProgress) {
    return;
  }
  _mediator.addAccountOperationInProgress = YES;
  __weak __typeof(self) weakSelf = self;
  ShowSigninCommandCompletionCallback callback =
      ^(SigninCoordinatorResult result, SigninCompletionInfo* completionInfo) {
        __typeof(self) strongSelf = weakSelf;
        if (strongSelf) {
          strongSelf->_mediator.addAccountOperationInProgress = NO;
        }
      };
  ShowSigninCommand* command = [[ShowSigninCommand alloc]
      initWithOperation:AuthenticationOperation::kAddAccount
               identity:nil
            accessPoint:signin_metrics::AccessPoint::ACCESS_POINT_ACCOUNT_MENU
            promoAction:signin_metrics::PromoAction::
                            PROMO_ACTION_NO_SIGNIN_PROMO
               callback:callback];
  [_applicationHandler showSignin:command
               baseViewController:_navigationController];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [self.delegate acountMenuCoordinatorShouldStop:self];
  _navigationController = nil;
}

#pragma mark - SignoutActionSheetCoordinatorDelegate

- (void)signoutActionSheetCoordinatorPreventUserInteraction:
    (SignoutActionSheetCoordinator*)coordinator {
  _mediator.signOutFlowInProgress = YES;
}

- (void)signoutActionSheetCoordinatorAllowUserInteraction:
    (SignoutActionSheetCoordinator*)coordinator {
  _mediator.signOutFlowInProgress = NO;
}

#pragma mark - AccountMenuMediatorDelegate

- (void)mediatorWantsToBeDismissed:(AccountMenuMediator*)mediator {
  CHECK_EQ(mediator, _mediator);
  [self.delegate acountMenuCoordinatorShouldStop:self];
}

- (void)triggerSignoutWithTargetRect:(CGRect)targetRect
                          completion:(void (^)(BOOL success))completion {
  CHECK(!_mediator.signOutFlowInProgress &&
        !_mediator.addAccountOperationInProgress);
  CHECK(
      _authenticationService->HasPrimaryIdentity(signin::ConsentLevel::kSignin),
      base::NotFatalUntil::M130)
      << "There must be a signed-in account to view the menu and be able to "
         "switch accounts.";

  _signoutActionSheetCoordinator = [[SignoutActionSheetCoordinator alloc]
      initWithBaseViewController:_navigationController
                         browser:self.browser
                            rect:targetRect
                            view:_viewController.view
                      withSource:signin_metrics::ProfileSignout::
                                     kChangeAccountInAccountMenu];
  _signoutActionSheetCoordinator.delegate = self;
  _signoutActionSheetCoordinator.skipPostSignoutSnackbar = YES;

  __weak __typeof(self) weakSelf = self;
  _signoutActionSheetCoordinator.completion = ^(BOOL signoutSuccess) {
    [weakSelf stopSignoutActionSheetCoordinator];
    completion(signoutSuccess);
  };
  [_signoutActionSheetCoordinator start];
}

- (void)triggerSigninWithSystemIdentity:(id<SystemIdentity>)identity
                             completion:
                                 (void (^)(id<SystemIdentity> systemIdentity))
                                     completion {
  AuthenticationFlow* authenticationFlow = [[AuthenticationFlow alloc]
               initWithBrowser:self.browser
                      identity:identity
                   accessPoint:signin_metrics::AccessPoint::
                                   ACCESS_POINT_ACCOUNT_MENU
             postSignInActions:PostSignInActionSet({PostSignInAction::kNone})
      presentingViewController:_navigationController];

  [authenticationFlow startSignInWithCompletion:^(BOOL success) {
    completion(identity);
  }];
}

- (void)triggerAccountSwitchSnackbarWithIdentity:
    (id<SystemIdentity>)systemIdentity {
  NSString* accountName = systemIdentity.userGivenName
                              ? systemIdentity.userGivenName
                              : systemIdentity.userEmail;
  MDCSnackbarMessage* snackbarTitle = CreateSnackbarMessage(
      l10n_util::GetNSStringF(IDS_IOS_ACCOUNT_MENU_SWITCH_CONFIRMATION_TITLE,
                              base::SysNSStringToUTF16(accountName)));
  CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();
  id<SnackbarCommands> snackbarCommandsHandler =
      HandlerForProtocol(dispatcher, SnackbarCommands);
  [snackbarCommandsHandler showSnackbarMessage:snackbarTitle bottomOffset:0];
}

#pragma mark - SyncErrorSettingsCommandHandler

- (void)openPassphraseDialogWithModalPresentation:(BOOL)presentModally {
  if (presentModally) {
    SyncEncryptionPassphraseTableViewController* controllerToPresent =
        [[SyncEncryptionPassphraseTableViewController alloc]
            initWithBrowser:self.browser];
    controllerToPresent.presentModally = YES;
    UINavigationController* navigationController =
        [[UINavigationController alloc]
            initWithRootViewController:controllerToPresent];
    navigationController.modalPresentationStyle = UIModalPresentationFormSheet;
    [self configureHandlersForRootViewController:controllerToPresent];
    [_navigationController presentViewController:navigationController
                                        animated:YES
                                      completion:nil];
    return;
  }
  UIViewController<SettingsRootViewControlling>* controllerToPush;
  // If there was a sync error, prompt the user to enter the passphrase.
  // Otherwise, show the full encryption options.
  if (_syncService->GetUserSettings()->IsPassphraseRequired()) {
    controllerToPush = [[SyncEncryptionPassphraseTableViewController alloc]
        initWithBrowser:self.browser];
  } else {
    controllerToPush = [[SyncEncryptionTableViewController alloc]
        initWithBrowser:self.browser];
  }

  [self configureHandlersForRootViewController:controllerToPush];
  [_navigationController pushViewController:controllerToPush animated:YES];
}

- (void)openTrustedVaultReauthForFetchKeys {
  id<ApplicationCommands> applicationCommands =
      static_cast<id<ApplicationCommands>>(
          self.browser->GetCommandDispatcher());
  syncer::TrustedVaultUserActionTriggerForUMA trigger =
      syncer::TrustedVaultUserActionTriggerForUMA::kSettings;
  signin_metrics::AccessPoint accessPoint =
      signin_metrics::AccessPoint::ACCESS_POINT_ACCOUNT_MENU;
  [applicationCommands
      showTrustedVaultReauthForFetchKeysFromViewController:_navigationController
                                                   trigger:trigger
                                               accessPoint:accessPoint];
}

- (void)openTrustedVaultReauthForDegradedRecoverability {
  id<ApplicationCommands> applicationCommands =
      static_cast<id<ApplicationCommands>>(
          self.browser->GetCommandDispatcher());
  syncer::TrustedVaultUserActionTriggerForUMA trigger =
      syncer::TrustedVaultUserActionTriggerForUMA::kSettings;
  signin_metrics::AccessPoint accessPoint =
      signin_metrics::AccessPoint::ACCESS_POINT_ACCOUNT_MENU;
  [applicationCommands
      showTrustedVaultReauthForDegradedRecoverabilityFromViewController:
          _navigationController
                                                                trigger:trigger
                                                            accessPoint:
                                                                accessPoint];
}

- (void)openMDMErrodDialogWithSystemIdentity:(id<SystemIdentity>)identity {
  _authenticationService->ShowMDMErrorDialogForIdentity(identity);
}

- (void)openPrimaryAccountReauthDialog {
  id<ApplicationCommands> applicationCommands =
      static_cast<id<ApplicationCommands>>(
          self.browser->GetCommandDispatcher());
  ShowSigninCommand* signinCommand = [[ShowSigninCommand alloc]
      initWithOperation:AuthenticationOperation::kPrimaryAccountReauth
            accessPoint:signin_metrics::AccessPoint::ACCESS_POINT_ACCOUNT_MENU];
  [applicationCommands showSignin:signinCommand
               baseViewController:_navigationController];
}

#pragma mark - Private

- (void)stopAccountsCoordinator {
  [_accountsCoordinator stop];
  _accountsCoordinator = nil;
}

- (void)resetAccountDetailsControllerDismissCallback {
  _accountDetailsControllerDismissCallback.Reset();
}

- (void)configureHandlersForRootViewController:
    (id<SettingsRootViewControlling>)controller {
  CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();
  controller.applicationHandler =
      HandlerForProtocol(dispatcher, ApplicationCommands);
  controller.browserHandler = HandlerForProtocol(dispatcher, BrowserCommands);
  controller.settingsHandler = HandlerForProtocol(dispatcher, SettingsCommands);
  controller.snackbarHandler = HandlerForProtocol(dispatcher, SnackbarCommands);
}

- (void)stopSignoutActionSheetCoordinator {
  [_signoutActionSheetCoordinator stop];
  _signoutActionSheetCoordinator = nil;
}

@end
