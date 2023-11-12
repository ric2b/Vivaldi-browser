// Copyright 2022 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_SETTINGS_SYNC_VIVALDI_SYNC_SETTINGS_VIEW_CONTROLLER_MODEL_DELEGATE_H_
#define IOS_UI_SETTINGS_SYNC_VIVALDI_SYNC_SETTINGS_VIEW_CONTROLLER_MODEL_DELEGATE_H_

#import <UIKit/UIKit.h>

#import "ios/ui/settings/sync/vivaldi_sync_settings_consumer.h"

@protocol VivaldiSyncSettingsViewControllerModelDelegate

- (void)vivaldiSyncSettingsViewControllerLoadModel:
    (id<VivaldiSyncSettingsConsumer>)controller;

@end

#endif  // IOS_UI_SETTINGS_SYNC_VIVALDI_SYNC_SETTINGS_VIEW_CONTROLLER_MODEL_DELEGATE_H_
