// Copyright 2022 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_SETTINGS_SYNC_VIVALDI_SYNC_LOGIN_VIEW_CONTROLLER_H_
#define IOS_UI_SETTINGS_SYNC_VIVALDI_SYNC_LOGIN_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/ui/settings/sync/settings_root_table_view_controller+vivaldi.h"

@class VivaldiSyncLoginViewController;

@protocol ModalPageCommands;
@protocol VivaldiSyncLoginViewControllerDelegate

// Called when the view controller is removed from its parent.
- (void)vivaldiSyncLoginViewControllerWasRemoved:
    (VivaldiSyncLoginViewController*)controller;

// Log In
- (void)logInButtonPressed:(NSString*)username
                  password:(NSString*)password
                deviceName:(NSString*)deviceName
              savePassword:(BOOL)savePassword;

- (void)createAccountLinkPressed;

- (void)dismissVivaldiSyncLoginViewController;
@end

@interface VivaldiSyncLoginViewController : SettingsRootTableViewController

@property(nonatomic, weak) id<VivaldiSyncLoginViewControllerDelegate> delegate;

// Initializes the view controller, configured with |style|. The default
// ChromeTableViewStyler will be used.
- (instancetype)initWithModalPageHandler:(id<ModalPageCommands>)modalPageHandler
                                   style:(UITableViewStyle)style
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

- (void)loginFailed:(NSString*)errorMessage;

// Setup the cancel button on the navigation's left bar button.
- (void)setupLeftCancelButton;

@end

#endif  // IOS_UI_SETTINGS_SYNC_VIVALDI_SYNC_LOGIN_VIEW_CONTROLLER_H_
