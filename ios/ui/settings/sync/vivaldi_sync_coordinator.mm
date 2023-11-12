// Copyright 2022 Vivaldi Technologies. All rights reserved.

#import "ios/ui/settings/sync/vivaldi_sync_coordinator.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/table_view/table_view_utils.h"
#import "ios/sync/vivaldi_sync_service_factory.h"
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
  VivaldiSyncLoginViewControllerDelegate,
  VivaldiSyncEncryptionPasswordViewControllerDelegate,
  VivaldiSyncSettingsViewControllerDelegate,
  VivaldiSyncSettingsCommandHandler> {
}

@property(nonatomic, strong)
    VivaldiSyncLoginViewController* syncLoginViewController;
@property(nonatomic, strong) VivaldiSyncEncryptionPasswordViewController*
        syncEncryptionPasswordViewController;
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
  self.mediator = [[VivaldiSyncMediator alloc]
                    initWithAccountManager:account_manager
                    syncService:sync_service];
  self.mediator.commandHandler = self;

  [self.mediator startMediating];
}

- (void)stop {
  [self.mediator disconnect];
  self.delegate = nil;
  self.syncLoginViewController = nil;
  self.syncSettingsViewController = nil;
  self.syncEncryptionPasswordViewController = nil;
  self.mediator = nil;
}

#pragma mark - VivaldiSyncSettingsCommandHandler

- (void)showActivateAccountView {
  // TODO(tomas@vivaldi.com): Implement create account flow
}

- (void)showSyncEncryptionPasswordView {
  if ([self.baseNavigationController.viewControllers.lastObject
      isKindOfClass:[VivaldiSyncEncryptionPasswordViewController class]]) {
    return;
  }
  NSMutableArray* controllers = [self removeSyncViewsFromControllers:
      self.baseNavigationController.viewControllers];

  if (!self.syncEncryptionPasswordViewController) {
    self.syncEncryptionPasswordViewController =[
        [VivaldiSyncEncryptionPasswordViewController alloc]
            initWithStyle:ChromeTableViewStyle()
    ];
  }
  self.syncEncryptionPasswordViewController.delegate = self;
  self.mediator.settingsConsumer = nil;
  [controllers addObject:self.syncEncryptionPasswordViewController];
  [self.baseNavigationController setViewControllers:controllers animated:YES];
}

- (void)showSyncSettingsView {
  if ([self.baseNavigationController.viewControllers.lastObject
      isKindOfClass:[VivaldiSyncSettingsViewController class]]) {
    return;
  }
  NSMutableArray* controllers = [self removeSyncViewsFromControllers:
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
  NSMutableArray* controllers = [self removeSyncViewsFromControllers:
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

- (void)loginFailed {
  if (!self.syncLoginViewController)
    return;
  [self.syncLoginViewController loginFailed];
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

#pragma mark - Private Methods

- (void)decryptButtonPressed:(NSString*)encryptionPassword {
  if (!encryptionPassword) {
    return;
  }
  [self.mediator
      setEncryptionPassword:base::SysNSStringToUTF8(encryptionPassword)];
}

- (NSMutableArray*)removeSyncViewsFromControllers:(NSArray*)controllers {
  NSMutableArray* new_controllers = [[NSMutableArray alloc] init];
  for (SettingsRootTableViewController* c in controllers) {
    if (c && !(
      [c isKindOfClass:[VivaldiSyncSettingsViewController class]] ||
      [c isKindOfClass:[VivaldiSyncEncryptionPasswordViewController class]] ||
      [c isKindOfClass:[VivaldiSyncLoginViewController class]])) {
      [new_controllers addObject:c];
    }
  }
  return new_controllers;
}

@end
