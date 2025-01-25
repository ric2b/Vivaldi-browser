// Copyright 2022-2023 Vivaldi Technologies. All rights reserved.

#import "ios/ui/settings/sync/vivaldi_sync_coordinator.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/ui/modal_page/modal_page_commands.h"
#import "ios/ui/modal_page/modal_page_coordinator.h"
#import "ios/ui/settings/sync/vivaldi_sync_activate_account_view_controller.h"
#import "ios/ui/settings/sync/vivaldi_sync_create_account_password_view_controller.h"
#import "ios/ui/settings/sync/vivaldi_sync_create_account_user_view_controller.h"
#import "ios/ui/settings/sync/vivaldi_sync_create_encryption_password_view_controller.h"
#import "ios/ui/settings/sync/vivaldi_sync_encryption_password_view_controller.h"
#import "ios/ui/settings/sync/vivaldi_sync_login_view_controller.h"
#import "ios/ui/settings/sync/vivaldi_sync_mediator.h"
#import "ios/ui/settings/sync/vivaldi_sync_settings_command_handler.h"
#import "ios/ui/settings/sync/vivaldi_sync_settings_view_controller.h"
#import "ios/vivaldi_account/vivaldi_account_manager_factory.h"
#import "sync/vivaldi_sync_service_impl.h"
#import "vivaldi_account/vivaldi_account_manager.h"

@interface VivaldiSyncCoordinator () <
    VivaldiSyncActivateAccountViewControllerDelegate,
    VivaldiSyncCreateAccountUserViewControllerDelegate,
    VivaldiSyncCreateAccountPasswordViewControllerDelegate,
    VivaldiSyncCreateEncryptionPasswordViewControllerDelegate,
    VivaldiSyncEncryptionPasswordViewControllerDelegate,
    VivaldiSyncLoginViewControllerDelegate,
    VivaldiSyncSettingsViewControllerDelegate,
    VivaldiSyncSettingsCommandHandler> {
}

@property(nonatomic, strong) ModalPageCoordinator* modalPageCoordinator;
@property(nonatomic, strong)
    VivaldiSyncActivateAccountViewController* syncActivateAccountViewController;
@property(nonatomic, strong) VivaldiSyncCreateAccountUserViewController*
    syncCreateAccountUserViewController;
@property(nonatomic, strong) VivaldiSyncCreateAccountPasswordViewController*
    syncCreateAccountPasswordViewController;
@property(nonatomic, strong) VivaldiSyncEncryptionPasswordViewController*
    syncEncryptionPasswordViewController;
@property(nonatomic, strong) VivaldiSyncCreateEncryptionPasswordViewController*
    syncCreateEncryptionPasswordViewController;
@property(nonatomic, strong)
    VivaldiSyncLoginViewController* syncLoginViewController;
@property(nonatomic, strong)
    VivaldiSyncSettingsViewController* syncSettingsViewController;
@property(nonatomic, strong) VivaldiSyncMediator* mediator;

// Used to present sync view with create account page on top when YES.
@property(nonatomic, assign) BOOL showCreateAccount;

@end

@implementation VivaldiSyncCoordinator

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
                               showCreateAccount:(BOOL)showCreateAccount {
  self =
      [super initWithBaseViewController:navigationController browser:browser];
  if (self) {
    _baseNavigationController = navigationController;
    _showCreateAccount = showCreateAccount;
  }
  return self;
}

- (void)dealloc {
  DCHECK(!self.delegate);
  DCHECK(!self.syncLoginViewController);
  DCHECK(!self.syncCreateEncryptionPasswordViewController);
  DCHECK(!self.syncEncryptionPasswordViewController);
  DCHECK(!self.syncSettingsViewController);
  DCHECK(!self.syncCreateAccountUserViewController);
  DCHECK(!self.syncCreateAccountPasswordViewController);
  DCHECK(!self.syncActivateAccountViewController);
  DCHECK(!self.mediator);
}

- (void)start {
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(self.browser->GetProfile());
  vivaldi::VivaldiAccountManager* account_manager =
      vivaldi::VivaldiAccountManagerFactory::GetForProfile(
          self.browser->GetProfile());
  PrefService* pref_service =
      ProfileIOS::FromBrowserState(self.browser->GetProfile())->GetPrefs();
  self.mediator =
      [[VivaldiSyncMediator alloc] initWithAccountManager:account_manager
                                              syncService:sync_service
                                              prefService:pref_service];
  self.mediator.commandHandler = self;

  [self.mediator startMediating];
}

- (void)stop {
  [self.mediator disconnect];
  self.delegate = nil;
  self.syncLoginViewController = nil;
  self.syncSettingsViewController = nil;
  self.syncCreateEncryptionPasswordViewController = nil;
  self.syncEncryptionPasswordViewController = nil;
  self.syncCreateAccountUserViewController = nil;
  self.syncCreateAccountPasswordViewController = nil;
  self.syncActivateAccountViewController = nil;
  self.mediator = nil;
  if (self.showCancelButton) {
    [self.baseNavigationController dismissViewControllerAnimated:YES
                                                      completion:nil];
  }
}

#pragma mark - VivaldiSyncSettingsCommandHandler

- (void)resetViewControllers {
  self.syncLoginViewController = nil;
  self.syncSettingsViewController = nil;
  self.syncCreateEncryptionPasswordViewController = nil;
  self.syncEncryptionPasswordViewController = nil;
  self.syncCreateAccountUserViewController = nil;
  self.syncCreateAccountPasswordViewController = nil;
  self.syncActivateAccountViewController = nil;
}

- (void)showActivateAccountView {
  if ([self.baseNavigationController.viewControllers.lastObject
          isKindOfClass:[VivaldiSyncActivateAccountViewController class]]) {
    return;
  }

  if (!self.syncActivateAccountViewController) {
    self.syncActivateAccountViewController =
        [[VivaldiSyncActivateAccountViewController alloc]
            initWithStyle:ChromeTableViewStyle()];
  }
  self.syncActivateAccountViewController.delegate = self;
  self.mediator.settingsConsumer = nil;

  if ([self.baseNavigationController.viewControllers.lastObject
          isKindOfClass:[VivaldiSyncCreateAccountPasswordViewController
                            class]]) {
    [self.baseNavigationController
        pushViewController:self.syncActivateAccountViewController
                  animated:YES];
  } else {
    NSMutableArray* controllers = [self
        removeSyncViewsFromArray:self.baseNavigationController.viewControllers];
    [controllers addObject:self.syncActivateAccountViewController];
    [self.baseNavigationController setViewControllers:controllers animated:YES];
  }
}

- (void)showSyncCreateAccountUserView {
  if ([self.baseNavigationController.viewControllers.lastObject
          isKindOfClass:[VivaldiSyncCreateAccountUserViewController class]]) {
    return;
  }

  if ([self.baseNavigationController.viewControllers.lastObject
          isKindOfClass:[VivaldiSyncCreateAccountPasswordViewController
                            class]]) {
    [self.baseNavigationController popViewControllerAnimated:YES];
    return;
  }

  if (!self.syncCreateAccountUserViewController) {
    self.syncCreateAccountUserViewController =
        [[VivaldiSyncCreateAccountUserViewController alloc]
            initWithStyle:ChromeTableViewStyle()];
  }
  self.syncCreateAccountUserViewController.shouldHideDoneButton = YES;
  self.syncCreateAccountUserViewController.delegate = self;
  self.mediator.settingsConsumer = nil;

  if ([self.baseNavigationController.viewControllers.lastObject
          isKindOfClass:[VivaldiSyncLoginViewController class]]) {
    [self.baseNavigationController
        pushViewController:self.syncCreateAccountUserViewController
                  animated:YES];
  } else {
    NSMutableArray* controllers = [self
        removeSyncViewsFromArray:self.baseNavigationController.viewControllers];
    [controllers addObject:self.syncCreateAccountUserViewController];
    [self.baseNavigationController setViewControllers:controllers animated:YES];
  }
}

- (void)showSyncCreateAccountPasswordView {
  if ([self.baseNavigationController.viewControllers.lastObject
          isKindOfClass:[VivaldiSyncCreateAccountPasswordViewController
                            class]]) {
    return;
  }

  if (!self.syncCreateAccountPasswordViewController) {
    [self.browser->GetCommandDispatcher()
        startDispatchingToTarget:self
                     forProtocol:@protocol(ModalPageCommands)];
    id<ModalPageCommands> modalPageHandler = HandlerForProtocol(
        self.browser->GetCommandDispatcher(), ModalPageCommands);

    self.syncCreateAccountPasswordViewController =
        [[VivaldiSyncCreateAccountPasswordViewController alloc]
            initWithModalPageHandler:modalPageHandler
                               style:ChromeTableViewStyle()];
  }
  self.syncCreateAccountPasswordViewController.shouldHideDoneButton = YES;
  self.syncCreateAccountPasswordViewController.delegate = self;
  self.mediator.settingsConsumer = nil;

  if ([self.baseNavigationController.viewControllers.lastObject
          isKindOfClass:[VivaldiSyncCreateAccountUserViewController class]]) {
    [self.baseNavigationController
        pushViewController:self.syncCreateAccountPasswordViewController
                  animated:YES];
  } else {
    NSMutableArray* controllers = [self
        removeSyncViewsFromArray:self.baseNavigationController.viewControllers];
    [controllers addObject:self.syncCreateAccountPasswordViewController];
    [self.baseNavigationController setViewControllers:controllers animated:YES];
  }
}

- (void)showSyncEncryptionPasswordView {
  if ([self.baseNavigationController.viewControllers.lastObject
          isKindOfClass:[VivaldiSyncEncryptionPasswordViewController class]]) {
    return;
  }
  NSMutableArray* controllers = [self
      removeSyncViewsFromArray:self.baseNavigationController.viewControllers];

  if (!self.syncEncryptionPasswordViewController) {
    self.syncEncryptionPasswordViewController =
        [[VivaldiSyncEncryptionPasswordViewController alloc]
            initWithStyle:ChromeTableViewStyle()];
  }

  self.syncEncryptionPasswordViewController.shouldHideDoneButton = YES;
  self.syncEncryptionPasswordViewController.delegate = self;
  self.mediator.settingsConsumer = nil;
  [controllers addObject:self.syncEncryptionPasswordViewController];
  [self.baseNavigationController setViewControllers:controllers animated:YES];
}

- (void)showSyncCreateEncryptionPasswordView {
  if ([self.baseNavigationController.viewControllers.lastObject
          isKindOfClass:[VivaldiSyncCreateEncryptionPasswordViewController
                            class]]) {
    return;
  }
  NSMutableArray* controllers = [self
      removeSyncViewsFromArray:self.baseNavigationController.viewControllers];

  if (!self.syncCreateEncryptionPasswordViewController) {
    self.syncCreateEncryptionPasswordViewController =
        [[VivaldiSyncCreateEncryptionPasswordViewController alloc]
            initWithStyle:ChromeTableViewStyle()];
  }
  self.syncCreateEncryptionPasswordViewController.shouldHideDoneButton = YES;
  self.syncCreateEncryptionPasswordViewController.delegate = self;
  self.mediator.settingsConsumer = nil;
  [controllers addObject:self.syncCreateEncryptionPasswordViewController];
  [self.baseNavigationController setViewControllers:controllers animated:YES];
}

- (void)showSyncSettingsView {
  if ([self.baseNavigationController.viewControllers.lastObject
          isKindOfClass:[VivaldiSyncSettingsViewController class]]) {
    return;
  }
  NSMutableArray* controllers = [self
      removeSyncViewsFromArray:self.baseNavigationController.viewControllers];

  if (!self.syncSettingsViewController) {
    self.syncSettingsViewController = [[VivaldiSyncSettingsViewController alloc]
        initWithStyle:ChromeTableViewStyle()];
  }
  self.syncSettingsViewController.delegate = self;
  self.syncSettingsViewController.modelDelegate = self.mediator;
  self.syncSettingsViewController.serviceDelegate = self.mediator;
  self.syncSettingsViewController.applicationCommandsHandler =
      HandlerForProtocol(
          self.browser->GetCommandDispatcher(), ApplicationCommands);
  self.mediator.settingsConsumer = self.syncSettingsViewController;
  [controllers addObject:self.syncSettingsViewController];
  [self.baseNavigationController setViewControllers:controllers animated:YES];
}

- (void)showSyncLoginView {
  // When showCreateAccount is YES we will put the Login view to the stack first
  // and add the Create account view next. This allows going back to login view
  // from create account view. This state is only expected when user is not
  // logged in and opens the create account page from tab switcher synced tabs
  // empty state.
  if (_showCreateAccount) {
    [self addSyncLoginViewControllerToNavigationStack];
    [self showSyncCreateAccountUserView];
    _showCreateAccount = NO;
    return;
  }

  if ([self.baseNavigationController.viewControllers.lastObject
          isKindOfClass:[VivaldiSyncLoginViewController class]]) {
    return;
  }

  if ([self.baseNavigationController.viewControllers.lastObject
          isKindOfClass:[VivaldiSyncCreateAccountUserViewController class]]) {
    [self.baseNavigationController popViewControllerAnimated:YES];
    return;
  }

  [self addSyncLoginViewControllerToNavigationStack];
}

- (void)loginFailed:(NSString*)errorMessage {
  if (!self.syncLoginViewController) {
    [self showSyncLoginView];
  }
  [self.syncLoginViewController loginFailed:errorMessage];
}

- (void)createAccountFailed:(NSString*)errorCode {
  if (!self.syncCreateAccountPasswordViewController)
    return;
  [self.syncCreateAccountPasswordViewController createAccountFailed:errorCode];
}

#pragma mark - VivaldiSyncLoginViewControllerDelegate

- (void)vivaldiSyncActivateAccountViewControllerWasRemoved:
    (VivaldiSyncActivateAccountViewController*)controller {
  DCHECK_EQ(self.syncActivateAccountViewController, controller);
  if ([self.baseNavigationController.viewControllers.lastObject
          isKindOfClass:[VivaldiSyncCreateEncryptionPasswordViewController
                            class]]) {
    return;
  }
  if ([self.baseNavigationController.viewControllers.lastObject
          isKindOfClass:[VivaldiSyncCreateAccountPasswordViewController
                            class]]) {
    return;
  }
  [self.delegate vivaldiSyncCoordinatorWasRemoved:self];
}

- (void)requestPendingRegistrationLogin {
  [self.mediator requestPendingRegistrationLogin];
}

- (NSString*)getPendingRegistrationUsername {
  return [self.mediator getPendingRegistrationUsername];
}

- (NSString*)getPendingRegistrationEmail {
  return [self.mediator getPendingRegistrationEmail];
}

- (void)clearPendingRegistration {
  [self.mediator clearPendingRegistration];
}

#pragma mark - VivaldiSyncLoginViewControllerDelegate

- (void)vivaldiSyncLoginViewControllerWasRemoved:
    (VivaldiSyncLoginViewController*)controller {
  DCHECK_EQ(self.syncLoginViewController, controller);
  if ([self.baseNavigationController.viewControllers.lastObject
          isKindOfClass:[VivaldiSyncEncryptionPasswordViewController class]]) {
    return;
  }
  if ([self.baseNavigationController.viewControllers.lastObject
          isKindOfClass:[VivaldiSyncCreateEncryptionPasswordViewController
                            class]]) {
    return;
  }
  if ([self.baseNavigationController.viewControllers.lastObject
          isKindOfClass:[VivaldiSyncSettingsViewController class]]) {
    return;
  }
  if ([self.baseNavigationController.viewControllers.lastObject
          isKindOfClass:[VivaldiSyncCreateAccountUserViewController class]]) {
    return;
  }
  if ([self.baseNavigationController.viewControllers.lastObject
          isKindOfClass:[VivaldiSyncActivateAccountViewController class]]) {
    return;
  }
  [self.delegate vivaldiSyncCoordinatorWasRemoved:self];
}

- (void)logInButtonPressed:(NSString*)username
                  password:(NSString*)password
                deviceName:(NSString*)deviceName
              savePassword:(BOOL)savePassword {
  if (!username || !password) {
    return;
  }
  [self.mediator login:base::SysNSStringToUTF8(username)
              password:base::SysNSStringToUTF8(password)
            deviceName:deviceName
         save_password:savePassword];
}

- (void)createAccountLinkPressed {
  [self showSyncCreateAccountUserView];
}

- (void)dismissVivaldiSyncLoginViewController {
  if (!self.syncLoginViewController)
    return;
  [self.delegate vivaldiSyncCoordinatorWasRemoved:self];
}

#pragma mark - VivaldiSyncSettingsViewControllerDelegate

- (void)vivaldiSyncSettingsViewControllerWasRemoved:
    (VivaldiSyncSettingsViewController*)controller {
  DCHECK_EQ(self.syncSettingsViewController, controller);
  if ([self.baseNavigationController.viewControllers.lastObject
          isKindOfClass:[VivaldiSyncEncryptionPasswordViewController class]]) {
    return;
  }
  if ([self.baseNavigationController.viewControllers.lastObject
          isKindOfClass:[VivaldiSyncLoginViewController class]]) {
    return;
  }
  [self.delegate vivaldiSyncCoordinatorWasRemoved:self];
}

#pragma mark - VivaldiSyncCreateEncryptionPasswordViewControllerDelegate

- (void)vivaldiSyncCreateEncryptionPasswordViewControllerWasRemoved:
    (VivaldiSyncCreateEncryptionPasswordViewController*)controller {
  DCHECK_EQ(self.syncCreateEncryptionPasswordViewController, controller);
  if ([self.baseNavigationController.viewControllers.lastObject
          isKindOfClass:[VivaldiSyncSettingsViewController class]]) {
    return;
  }
  if ([self.baseNavigationController.viewControllers.lastObject
          isKindOfClass:[VivaldiSyncLoginViewController class]]) {
    return;
  }
  [self.delegate vivaldiSyncCoordinatorWasRemoved:self];
}

- (void)saveEncryptionKeyButtonPressed:(NSString*)encryptionPassword
                     completionHandler:
                         (void (^)(BOOL success))completionHandler {
  if (!encryptionPassword) {
    return;
  }
  BOOL success = [self.mediator
      setEncryptionPassword:base::SysNSStringToUTF8(encryptionPassword)];
  if (completionHandler) {
    completionHandler(success);
  }
}

#pragma mark - VivaldiSyncEncryptionPasswordViewControllerDelegate

- (void)vivaldiSyncEncryptionPasswordViewControllerWasRemoved:
    (VivaldiSyncEncryptionPasswordViewController*)controller {
  // DCHECK_EQ(self.syncEncryptionPasswordViewController, controller);
  if ([self.baseNavigationController.viewControllers.lastObject
          isKindOfClass:[VivaldiSyncSettingsViewController class]]) {
    return;
  }
  if ([self.baseNavigationController.viewControllers.lastObject
          isKindOfClass:[VivaldiSyncLoginViewController class]]) {
    return;
  }
  [self.delegate vivaldiSyncCoordinatorWasRemoved:self];
}

- (void)decryptButtonPressed:(NSString*)encryptionPassword
           completionHandler:(void (^)(BOOL success))completionHandler {
  if (!encryptionPassword) {
    return;
  }
  BOOL success = [self.mediator
      setEncryptionPassword:base::SysNSStringToUTF8(encryptionPassword)];
  if (completionHandler) {
    completionHandler(success);
  }
}

- (void)importPasskey:(NSURL*)fileSelected
    completionHandler:(void (^)(NSString* errorMessage))completionHandler {
  if (!fileSelected) {
    return;
  }
  [self.mediator importEncryptionPassword:fileSelected
                        completionHandler:completionHandler];
}

- (void)logOutButtonPressed {
  [self.mediator logOutButtonPressed];
}

- (void)updateDeviceName:(NSString*)deviceName {
  [self.mediator updateDeviceName:deviceName];
}

- (void)deleteRemoteData {
  [self.mediator clearSyncDataWithNoWarning];
}

#pragma mark - VivaldiSyncCreateAccountUserViewControllerDelegate

- (void)vivaldiSyncCreateAccountUserViewControllerWasRemoved:
    (VivaldiSyncCreateAccountUserViewController*)controller {
  DCHECK_EQ(self.syncCreateAccountUserViewController, controller);
  if ([self.baseNavigationController.viewControllers.lastObject
          isKindOfClass:[VivaldiSyncCreateEncryptionPasswordViewController
                            class]]) {
    return;
  }
  if ([self.baseNavigationController.viewControllers.lastObject
          isKindOfClass:[VivaldiSyncSettingsViewController class]]) {
    return;
  }
  if ([self.baseNavigationController.viewControllers.lastObject
          isKindOfClass:[VivaldiSyncLoginViewController class]]) {
    return;
  }
  if ([self.baseNavigationController.viewControllers.lastObject
          isKindOfClass:[VivaldiSyncActivateAccountViewController class]]) {
    return;
  }
  [self.delegate vivaldiSyncCoordinatorWasRemoved:self];
}

- (void)nextButtonPressed:(NSString*)username
                      age:(int)age
     recoveryEmailAddress:(NSString*)recoveryEmailAddress {
  [self.mediator storeUsername:username age:age email:recoveryEmailAddress];
  [self showSyncCreateAccountPasswordView];
}

- (void)logInLinkPressed {
  [self showSyncLoginView];
}

#pragma mark - VivaldiSyncCreateAccountPasswordViewControllerDelegate

- (void)vivaldiSyncCreateAccountPasswordViewControllerWasRemoved:
    (VivaldiSyncCreateAccountPasswordViewController*)controller {
  DCHECK_EQ(self.syncCreateAccountPasswordViewController, controller);
  if ([self.baseNavigationController.viewControllers.lastObject
          isKindOfClass:[VivaldiSyncCreateEncryptionPasswordViewController
                            class]]) {
    return;
  }
  if ([self.baseNavigationController.viewControllers.lastObject
          isKindOfClass:[VivaldiSyncSettingsViewController class]]) {
    return;
  }
  if ([self.baseNavigationController.viewControllers.lastObject
          isKindOfClass:[VivaldiSyncLoginViewController class]]) {
    return;
  }
  if ([self.baseNavigationController.viewControllers.lastObject
          isKindOfClass:[VivaldiSyncCreateAccountUserViewController class]]) {
    return;
  }
  if ([self.baseNavigationController.viewControllers.lastObject
          isKindOfClass:[VivaldiSyncActivateAccountViewController class]]) {
    return;
  }
  [self.delegate vivaldiSyncCoordinatorWasRemoved:self];
}

- (void)createAccountButtonPressed:(NSString*)password
                        deviceName:(NSString*)deviceName
                   wantsNewsletter:(BOOL)wantsNewsletter {
  [self.mediator createAccount:password
                    deviceName:deviceName
               wantsNewsletter:wantsNewsletter];
}

#pragma mark - ModalPageCommands

- (void)showModalPage:(NSURL*)url title:(NSString*)title {
  self.modalPageCoordinator = [[ModalPageCoordinator alloc]
      initWithBaseViewController:self.baseNavigationController.viewControllers
                                     .lastObject
                         browser:self.browser
                         pageURL:url
                           title:title];
  [self.modalPageCoordinator start];
}

- (void)closeModalPage {
  [self.modalPageCoordinator stop];
  self.modalPageCoordinator = nil;
}

#pragma mark - Private Methods

- (void)addSyncLoginViewControllerToNavigationStack {
  if (!self.syncLoginViewController) {
    [self.browser->GetCommandDispatcher()
        startDispatchingToTarget:self
                     forProtocol:@protocol(ModalPageCommands)];
    id<ModalPageCommands> modalPageHandler = HandlerForProtocol(
        self.browser->GetCommandDispatcher(), ModalPageCommands);
    self.syncLoginViewController = [[VivaldiSyncLoginViewController alloc]
        initWithModalPageHandler:modalPageHandler
                           style:ChromeTableViewStyle()];
    if (self.showCancelButton) {
      [self.syncLoginViewController setupLeftCancelButton];
    }
    self.syncLoginViewController.shouldHideDoneButton = YES;
    self.syncLoginViewController.delegate = self;
  }
  self.mediator.settingsConsumer = nil;
  NSMutableArray* controllers = [self
      removeSyncViewsFromArray:self.baseNavigationController.viewControllers];
  [controllers addObject:self.syncLoginViewController];
  [self.baseNavigationController setViewControllers:controllers animated:YES];
}

- (NSMutableArray*)removeSyncViewsFromArray:(NSArray*)controllers {
  NSMutableArray* new_controllers = [[NSMutableArray alloc] init];
  for (SettingsRootTableViewController* c in controllers) {
    if (c &&
        !([c isKindOfClass:[VivaldiSyncSettingsViewController class]] ||
          [c isKindOfClass:[VivaldiSyncCreateEncryptionPasswordViewController
                               class]] ||
          [c isKindOfClass:[VivaldiSyncEncryptionPasswordViewController
                               class]] ||
          [c isKindOfClass:[VivaldiSyncCreateAccountPasswordViewController
                               class]] ||
          [c isKindOfClass:[VivaldiSyncCreateAccountUserViewController
                               class]] ||
          [c isKindOfClass:[VivaldiSyncActivateAccountViewController class]] ||
          [c isKindOfClass:[VivaldiSyncLoginViewController class]])) {
      [new_controllers addObject:c];
    }
  }
  return new_controllers;
}

@end
