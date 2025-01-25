// Copyright 2022 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_SETTINGS_SYNC_VIVALDI_SYNC_SETTINGS_VIEW_CONTROLLER_H_
#define IOS_UI_SETTINGS_SYNC_VIVALDI_SYNC_SETTINGS_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "components/sync/base/user_selectable_type.h"
#import "ios/chrome/browser/ui/settings/settings_root_table_view_controller.h"
#import "ios/ui/settings/sync/vivaldi_sync_settings_consumer.h"
#import "ios/ui/settings/sync/vivaldi_sync_settings_view_controller_model_delegate.h"
#import "ios/ui/settings/sync/vivaldi_sync_settings_view_controller_service_delegate.h"

@class VivaldiSyncSettingsViewController;
@protocol ApplicationCommands;

typedef NS_ENUM(NSInteger, SyncType) {
  SyncAll = 0,
  SyncSelected
};

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierSyncUserInfo = kSectionIdentifierEnumZero,
  SectionIdentifierSyncStatus,
  SectionIdentifierSyncItems,
  SectionIdentifierSyncStartSyncing,
  SectionIdentifierSyncEncryption,
  SectionIdentifierSyncSignOut,
  SectionIdentifierSyncDeleteData,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeSyncUserInfo = kItemTypeEnumZero,

  ItemTypeSyncStatus,
  ItemTypeSyncStatusFooter,

  ItemTypeSyncWhatSegmentedControl,
  ItemTypeSyncAllInfoTextbox,
  ItemTypeSyncBookmarksSwitch,
  ItemTypeSyncSettingsSwitch,
  ItemTypeSyncPasswordsSwitch,
  ItemTypeSyncAutofillSwitch,
  ItemTypeSyncTabsSwitch,
  ItemTypeSyncHistorySwitch,
  ItemTypeSyncReadingListSwitch,
  ItemTypeSyncNotesSwitch,

  ItemTypeStartSyncingButton,

  ItemTypeEncryptionPasswordButton,
  ItemTypeBackupRecoveryKeyButton,

  ItemTypeLogOutButton,
  ItemTypeDeleteDataButton,

  ItemTypeHeaderItem,
};

@protocol VivaldiSyncSettingsViewControllerDelegate

- (void)updateDeviceName:(NSString*)deviceName;

// Called when the view controller is removed from its parent.
- (void)vivaldiSyncSettingsViewControllerWasRemoved:
    (VivaldiSyncSettingsViewController*)controller;

@end

@interface VivaldiSyncSettingsViewController
    : SettingsRootTableViewController <VivaldiSyncSettingsConsumer,
                                      TableViewLinkHeaderFooterItemDelegate>

// ApplicationCommands handler.
@property(nonatomic, weak) id<ApplicationCommands> applicationCommandsHandler;

@property(nonatomic, weak)
    id<VivaldiSyncSettingsViewControllerDelegate> delegate;
@property(nonatomic, weak)
    id<VivaldiSyncSettingsViewControllerServiceDelegate> serviceDelegate;
@property(nonatomic, weak)
    id<VivaldiSyncSettingsViewControllerModelDelegate> modelDelegate;

@end

#endif  // IOS_UI_SETTINGS_SYNC_VIVALDI_SYNC_SETTINGS_VIEW_CONTROLLER_H_
