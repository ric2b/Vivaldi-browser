// Copyright 2023 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_SETTINGS_SYNC_VIVALDI_SYNC_CREATE_ACCOUNT_PASSWORD_VIEW_CONTROLLER_H_
#define IOS_UI_SETTINGS_SYNC_VIVALDI_SYNC_CREATE_ACCOUNT_PASSWORD_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/ui/settings/sync/settings_root_table_view_controller+vivaldi.h"

@class VivaldiSyncCreateAccountPasswordViewController;
@protocol ModalPageCommands;

@protocol VivaldiSyncCreateAccountPasswordViewControllerDelegate

// Called when the view controller is removed from its parent.
- (void)vivaldiSyncCreateAccountPasswordViewControllerWasRemoved:
    (VivaldiSyncCreateAccountPasswordViewController*)controller;

- (void)createAccountButtonPressed:(NSString*)password
                        deviceName:(NSString*)deviceName
                   wantsNewsletter:(BOOL)wantsNewsletter;

@end

@interface VivaldiSyncCreateAccountPasswordViewController :
    SettingsRootTableViewController

@property(nonatomic, weak)
    id<VivaldiSyncCreateAccountPasswordViewControllerDelegate> delegate;

- (instancetype)initWithModalPageHandler:(id<ModalPageCommands>)modalPageHandler
                                   style:(UITableViewStyle)style
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

- (void)createAccountFailed:(NSString*)errorCode;

@end

#endif  // IOS_UI_SETTINGS_SYNC_VIVALDI_SYNC_CREATE_ACCOUNT_PASSWORD_VIEW_CONTROLLER_H_
