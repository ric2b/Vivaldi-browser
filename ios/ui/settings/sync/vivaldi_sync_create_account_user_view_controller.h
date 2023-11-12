// Copyright 2023 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_SETTINGS_SYNC_VIVALDI_SYNC_CREATE_ACCOUNT_USER_VIEW_CONTROLLER_H_
#define IOS_UI_SETTINGS_SYNC_VIVALDI_SYNC_CREATE_ACCOUNT_USER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/ui/settings/sync/settings_root_table_view_controller+vivaldi.h"

@class VivaldiSyncCreateAccountUserViewController;

@protocol VivaldiSyncCreateAccountUserViewControllerDelegate

// Called when the view controller is removed from its parent.
- (void)vivaldiSyncCreateAccountUserViewControllerWasRemoved:
    (VivaldiSyncCreateAccountUserViewController*)controller;

- (void)nextButtonPressed:(NSString*)username
                      age:(int)age
     recoveryEmailAddress:(NSString*)recoveryEmailAddress;

- (void)logInLinkPressed;

@end

@interface VivaldiSyncCreateAccountUserViewController :
    SettingsRootTableViewController

@property(nonatomic, weak)
    id<VivaldiSyncCreateAccountUserViewControllerDelegate> delegate;

// Initializes the view controller, configured with |style|. The default
// ChromeTableViewStyler will be used.
- (instancetype)initWithStyle:(UITableViewStyle)style NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

@end

#endif  // IOS_UI_SETTINGS_SYNC_VIVALDI_SYNC_CREATE_ACCOUNT_USER_VIEW_CONTROLLER_H_
