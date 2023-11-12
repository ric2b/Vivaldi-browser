// Copyright 2022-2023 Vivaldi Technologies. All rights reserved.

#import "ios/ui/settings/sync/vivaldi_sync_coordinator.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/table_view/table_view_utils.h"
#import "ios/sync/vivaldi_sync_service_factory.h"
#import "ios/ui/modal_page/modal_page_commands.h"
#import "ios/ui/modal_page/modal_page_coordinator.h"
#import "ios/ui/settings/sync/vivaldi_sync_activate_account_view_controller.h"
#import "ios/ui/settings/sync/vivaldi_sync_create_account_password_view_controller.h"
#import "ios/ui/settings/sync/vivaldi_sync_create_account_user_view_controller.h"
#import "ios/ui/settings/sync/vivaldi_sync_encryption_password_view_controller.h"
#import "ios/ui/settings/sync/vivaldi_sync_login_view_controller.h"
#import "ios/ui/settings/sync/vivaldi_sync_mediator.h"
#import "ios/ui/settings/sync/vivaldi_sync_settings_command_handler.h"
#import "ios/ui/settings/sync/vivaldi_sync_settings_view_controller.h"
#import "ios/vivaldi_account/vivaldi_account_manager_factory.h"
#import "sync/vivaldi_sync_service_impl.h"
#import "vivaldi_account/vivaldi_account_manager.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface VivaldiSyncCoordinator () <
  VivaldiSyncActivateAccountViewControllerDelegate,
  VivaldiSyncCreateAccountUserViewControllerDelegate,
  VivaldiSyncCreateAccountPasswordViewControllerDelegate,
  VivaldiSyncEncryptionPasswordViewControllerDelegate,
  VivaldiSyncLoginViewControllerDelegate,
  VivaldiSyncSettingsViewControllerDelegate,
  VivaldiSyncSettingsCommandHandler> {
}

@property(nonatomic, strong) ModalPageCoordinator* modalPageCoordinator;
@property(nonatomic, strong) VivaldiSyncActivateAccountViewController*
        syncActivateAccountViewController;
@property(nonatomic, strong) VivaldiSyncCreateAccountUserViewController*
        syncCreateAccountUserViewController;
@property(nonatomic, strong) VivaldiSyncCreateAccountPasswordViewController*
        syncCreateAccountPasswordViewController;
@property(nonatomic, strong) VivaldiSyncEncryptionPasswordViewController*
        syncEncryptionPasswordViewController;
@property(nonatomic, strong)
    VivaldiSyncLoginViewController* syncLoginViewController;
@property(nonatomic, strong)
    VivaldiSyncSettingsViewController* syncSettingsViewController;
@property(nonatomic, strong) VivaldiSyncMediator* mediator;

@end

@implementation VivaldiSyncCoordinator

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    _baseNavigationController = navigationController;
  }
  return self;
}

- (void)dealloc {
  DCHECK(!self.delegate);
  DCHECK(!self.syncLoginViewController);
  DCHECK(!self.syncEncryptionPasswordViewController);
  DCHECK(!self.syncSettingsViewController);
  DCHECK(!self.syncCreateAccountUserViewController);
  DCHECK(!self.syncCreateAccountPasswordViewController);
  DCHECK(!self.syncActivateAccountViewController);
  DCHECK(!self.mediator);
}

- (void)start {
  vivaldi::VivaldiSyncServiceImpl* sync_service =
    reinterpret_cast<vivaldi::VivaldiSyncServiceImpl*>(
        vivaldi::VivaldiSyncServiceFactory::GetForBrowserState(
            self.browser->GetBrowserState()));
  vivaldi::VivaldiAccountManager* account_manager =
      vivaldi::VivaldiAccountManagerFactory::GetForBrowserState(
            self.browser->GetBrowserState());
  PrefService* pref_service = ChromeBrowserState::FromBrowserState(
            self.browser->GetBrowserState())->GetPrefs();
  self.mediator = [[VivaldiSyncMediator alloc]
                    initWithAccountManager:account_manager
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
  self.syncEncryptionPasswordViewController = nil;
  self.syncCreateAccountUserViewController = nil;
  self.syncCreateAccountPasswordViewController = nil;
  self.syncActivateAccountViewController = nil;
  self.mediator = nil;
}

#pragma mark - VivaldiSyncSettingsCommandHandler

- (void)showActivateAccountView {
  if ([self.baseNavigationController.viewControllers.lastObject
      isKindOfClass:[VivaldiSyncActivateAccountViewController class]]) {
    return;
  }

  if (!self.syncActivateAccountViewController) {
    self.syncActivateAccountViewController =[
        [VivaldiSyncActivateAccountViewController alloc]
            initWithStyle:ChromeTableViewStyle()
    ];
  }
  self.syncActivateAccountViewController.delegate = self;
  self.mediator.settingsConsumer = nil;


  if ([self.baseNavigationController.viewControllers.lastObject
      isKindOfClass:[VivaldiSyncCreateAccountPasswordViewController class]]) {
    [self.baseNavigationController
        pushViewController:self.syncActivateAccountViewController
                  animated:YES];
  } else {
    NSMutableArray* controllers = [self removeSyncViewsFromArray:
      self.baseNavigationController.viewControllers];
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
      isKindOfClass:[VivaldiSyncCreateAccountPasswordViewController class]]) {
    [self.baseNavigationController popViewControllerAnimated:YES];
    return;
  }

  if (!self.syncCreateAccountUserViewController) {
    self.syncCreateAccountUserViewController =[
        [VivaldiSyncCreateAccountUserViewController alloc]
            initWithStyle:ChromeTableViewStyle()
    ];
  }
  self.syncCreateAccountUserViewController.delegate = self;
  self.mediator.settingsConsumer = nil;


  if ([self.baseNavigationController.viewControllers.lastObject
      isKindOfClass:[VivaldiSyncLoginViewController class]]) {
    [self.baseNavigationController
        pushViewController:self.syncCreateAccountUserViewController
                  animated:YES];
  } else {
    NSMutableArray* controllers = [self removeSyncViewsFromArray:
      self.baseNavigationController.viewControllers];
    [controllers addObject:self.syncCreateAccountUserViewController];
    [self.baseNavigationController setViewControllers:controllers animated:YES];
  }
}

- (void)showSyncCreateAccountPasswordView {
  if ([self.baseNavigationController.viewControllers.lastObject
      isKindOfClass:[VivaldiSyncCreateAccountPasswordViewController class]]) {
    return;
  }

  if (!self.syncCreateAccountPasswordViewController) {
    [self.browser->GetCommandDispatcher()
      startDispatchingToTarget:self
                   forProtocol:@protocol(ModalPageCommands)];
      id<ModalPageCommands> modalPageHandler = HandlerForProtocol(
          self.browser->GetCommandDispatcher(), ModalPageCommands);

    self.syncCreateAccountPasswordViewController =[
        [VivaldiSyncCreateAccountPasswordViewController alloc]
            initWithModalPageHandler:modalPageHandler
                               style:ChromeTableViewStyle()
    ];
  }
  self.syncCreateAccountPasswordViewController.delegate = self;
  self.mediator.settingsConsumer = nil;

  if ([self.baseNavigationController.viewControllers.lastObject
      isKindOfClass:[VivaldiSyncCreateAccountUserViewController class]]) {
    [self.baseNavigationController
        pushViewController:self.syncCreateAccountPasswordViewController
                  animated:YES];
  } else {
    NSMutableArray* controllers = [self removeSyncViewsFromArray:
      self.baseNavigationController.viewControllers];
    [controllers addObject:self.syncCreateAccountPasswordViewController];
    [self.baseNavigationController setViewControllers:controllers animated:YES];
  }
}

- (void)showSyncEncryptionPasswordView:(BOOL)creatingPasscode {
  if ([self.baseNavigationController.viewControllers.lastObject
      isKindOfClass:[VivaldiSyncEncryptionPasswordViewController class]]) {
    return;
  }
  NSMutableArray* controllers = [self removeSyncViewsFromArray:
      self.baseNavigationController.viewControllers];

  if (!self.syncEncryptionPasswordViewController) {
    self.syncEncryptionPasswordViewController =[
        [VivaldiSyncEncryptionPasswordViewController alloc]
            initWithStyle:ChromeTableViewStyle()
    ];
  }

  self.syncEncryptionPasswordViewController.delegate = self;
  self.mediator.settingsConsumer = nil;
  [self.syncEncryptionPasswordViewController
      setCreatingPasscode:creatingPasscode];
  [controllers addObject:self.syncEncryptionPasswordViewController];
  [self.baseNavigationController setViewControllers:controllers animated:YES];
}

- (void)showSyncSettingsView {
  if ([self.baseNavigationController.viewControllers.lastObject
      isKindOfClass:[VivaldiSyncSettingsViewController class]]) {
    return;
  }
  NSMutableArray* controllers = [self removeSyncViewsFromArray:
      self.baseNavigationController.viewControllers];

  if (!self.syncSettingsViewController) {
    self.syncSettingsViewController =[[VivaldiSyncSettingsViewController alloc]
        initWithStyle:ChromeTableViewStyle()];
  }
  self.syncSettingsViewController.delegate = self;
  self.syncSettingsViewController.modelDelegate = self.mediator;
  self.syncSettingsViewController.serviceDelegate = self.mediator;
  self.syncSettingsViewController.applicationCommandsHandler =
      self.delegate.applicationCommandsHandler;
  self.mediator.settingsConsumer = self.syncSettingsViewController;
  [controllers addObject:self.syncSettingsViewController];
  [self.baseNavigationController setViewControllers:controllers animated:YES];
}

- (void)showSyncLoginView {
  if ([self.baseNavigationController.viewControllers.lastObject
      isKindOfClass:[VivaldiSyncLoginViewController class]]) {
    return;
  }
  if ([self.baseNavigationController.viewControllers.lastObject
      isKindOfClass:[VivaldiSyncCreateAccountUserViewController class]]) {
    [self.baseNavigationController popViewControllerAnimated:YES];
    return;
  }
  NSMutableArray* controllers = [self removeSyncViewsFromArray:
      self.baseNavigationController.viewControllers];

  if (!self.syncLoginViewController) {
    self.syncLoginViewController =[[VivaldiSyncLoginViewController alloc]
                                    initWithStyle:ChromeTableViewStyle()];
    self.syncLoginViewController.delegate = self;
  }
  self.mediator.settingsConsumer = nil;
  [controllers addObject:self.syncLoginViewController];
  [self.baseNavigationController setViewControllers:controllers animated:YES];
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
      isKindOfClass:[VivaldiSyncEncryptionPasswordViewController class]]) {
    return;
  }
  if ([self.baseNavigationController.viewControllers.lastObject
      isKindOfClass:[VivaldiSyncCreateAccountPasswordViewController class]]) {
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
              savePassword:(BOOL)savePassword {
  if (!username || !password) {
    return;
  }
  [self.mediator login:base::SysNSStringToUTF8(username)
                 password:base::SysNSStringToUTF8(password)
                 save_password:savePassword];
}

- (void)createAccountLinkPressed {
  [self showSyncCreateAccountUserView];
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


#pragma mark - VivaldiSyncEncryptionPasswordViewControllerDelegate

- (void)vivaldiSyncEncryptionPasswordViewControllerWasRemoved:
    (VivaldiSyncEncryptionPasswordViewController*)controller {
  DCHECK_EQ(self.syncEncryptionPasswordViewController, controller);
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

#pragma mark - VivaldiSyncCreateAccountUserViewControllerDelegate

- (void)vivaldiSyncCreateAccountUserViewControllerWasRemoved:
    (VivaldiSyncCreateAccountUserViewController*)controller {
  DCHECK_EQ(self.syncCreateAccountUserViewController, controller);
  if ([self.baseNavigationController.viewControllers.lastObject
      isKindOfClass:[VivaldiSyncEncryptionPasswordViewController class]]) {
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
  [self.mediator storeUsername:username
                           age:age
                         email:recoveryEmailAddress];
  [self showSyncCreateAccountPasswordView];
}

- (void)logInLinkPressed{
  [self showSyncLoginView];
}

#pragma mark - VivaldiSyncCreateAccountPasswordViewControllerDelegate

- (void)vivaldiSyncCreateAccountPasswordViewControllerWasRemoved:
    (VivaldiSyncCreateAccountPasswordViewController*)controller {
  DCHECK_EQ(self.syncCreateAccountPasswordViewController, controller);
  if ([self.baseNavigationController.viewControllers.lastObject
      isKindOfClass:[VivaldiSyncEncryptionPasswordViewController class]]) {
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
      initWithBaseViewController:
          self.baseNavigationController.viewControllers.lastObject
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

- (void)decryptButtonPressed:(NSString*)encryptionPassword {
  if (!encryptionPassword) {
    return;
  }
  [self.mediator
      setEncryptionPassword:base::SysNSStringToUTF8(encryptionPassword)];
}

- (NSMutableArray*)removeSyncViewsFromArray:(NSArray*)controllers {
  NSMutableArray* new_controllers = [[NSMutableArray alloc] init];
  for (SettingsRootTableViewController* c in controllers) {
    if (c && !(
      [c isKindOfClass:[VivaldiSyncSettingsViewController class]] ||
      [c isKindOfClass:[VivaldiSyncEncryptionPasswordViewController class]] ||
      [c isKindOfClass:[VivaldiSyncCreateAccountPasswordViewController class]] ||
      [c isKindOfClass:[VivaldiSyncCreateAccountUserViewController class]] ||
      [c isKindOfClass:[VivaldiSyncActivateAccountViewController class]] ||
      [c isKindOfClass:[VivaldiSyncLoginViewController class]])) {
      [new_controllers addObject:c];
    }
  }
  return new_controllers;
}

@end
