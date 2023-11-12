// Copyright 2023 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_SETTINGS_SYNC_VIVALDI_SYNC_ACTIVATE_ACCOUNT_VIEW_CONTROLLER_H_
#define IOS_UI_SETTINGS_SYNC_VIVALDI_SYNC_ACTIVATE_ACCOUNT_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/ui/settings/sync/settings_root_table_view_controller+vivaldi.h"

@class VivaldiSyncActivateAccountViewController;

@protocol VivaldiSyncActivateAccountViewControllerDelegate

// Called when the view controller is removed from its parent.
- (void)vivaldiSyncActivateAccountViewControllerWasRemoved:
    (VivaldiSyncActivateAccountViewController*)controller;

- (void)requestPendingRegistrationLogin;
- (NSString*)getPendingRegistrationUsername;
- (NSString*)getPendingRegistrationEmail;
- (void)clearPendingRegistration;

@end

@interface VivaldiSyncActivateAccountViewController
    : SettingsRootTableViewController

@property(nonatomic, weak)
    id<VivaldiSyncActivateAccountViewControllerDelegate> delegate;

// Initializes the view controller, configured with |style|. The default
// ChromeTableViewStyler will be used.
- (instancetype)initWithStyle:(UITableViewStyle)style NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

@end

#endif  // IOS_UI_SETTINGS_SYNC_VIVALDI_SYNC_ACTIVATE_ACCOUNT_VIEW_CONTROLLER_H_
