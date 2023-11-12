// Copyright 2022 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_SETTINGS_SYNC_VIVALDI_SYNC_SETTINGS_COMMAND_HANDLER_H_
#define IOS_UI_SETTINGS_SYNC_VIVALDI_SYNC_SETTINGS_COMMAND_HANDLER_H_

#import <UIKit/UIKit.h>

@protocol VivaldiSyncSettingsCommandHandler

- (void)showActivateAccountView;
- (void)showSyncEncryptionPasswordView;
- (void)showSyncSettingsView;
- (void)showSyncLoginView;
- (void)loginFailed;

@end

#endif  // IOS_UI_SETTINGS_SYNC_VIVALDI_SYNC_SETTINGS_COMMAND_HANDLER_H_
