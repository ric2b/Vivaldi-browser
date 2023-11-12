// Copyright 2022 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_SETTINGS_SYNC_VIVALDI_SYNC_LOGIN_VIEW_CONTROLLER_H_
#define IOS_UI_SETTINGS_SYNC_VIVALDI_SYNC_LOGIN_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/settings/settings_root_table_view_controller.h"

@class VivaldiSyncLoginViewController;

@protocol VivaldiSyncLoginViewControllerDelegate

// Called when the view controller is removed from its parent.
- (void)vivaldiSyncLoginViewControllerWasRemoved:
    (VivaldiSyncLoginViewController*)controller;

// Log In
- (void)logInButtonPressed:(NSString*)username
                  password:(NSString*)password
              savePassword:(BOOL)savePassword;

@end

@interface VivaldiSyncLoginViewController : SettingsRootTableViewController

@property(nonatomic, weak) id<VivaldiSyncLoginViewControllerDelegate> delegate;

// Initializes the view controller, configured with |style|. The default
// ChromeTableViewStyler will be used.
- (instancetype)initWithStyle:(UITableViewStyle)style NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

- (void)loginFailed;

@end

#endif  // IOS_UI_SETTINGS_SYNC_VIVALDI_SYNC_LOGIN_VIEW_CONTROLLER_H_
