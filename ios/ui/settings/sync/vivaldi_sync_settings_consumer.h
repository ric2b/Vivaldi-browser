// Copyright 2022 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_SETTINGS_SYNC_VIVALDI_SYNC_SETTINGS_CONSUMER_H_
#define IOS_UI_SETTINGS_SYNC_VIVALDI_SYNC_SETTINGS_CONSUMER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_model.h"

@protocol VivaldiSyncSettingsConsumer <NSObject>

// Returns the table view model.
@property(nonatomic, strong, readonly)
    TableViewModel<TableViewItem*>* tableViewModel;

// Returns the table view .
@property(nonatomic, strong, readonly) UITableView* tableView;

- (void)reloadItem:(TableViewItem*)item;
- (void)reloadSection:(NSInteger)sectionIdentifier;

@end

#endif  // IOS_UI_SETTINGS_SYNC_VIVALDI_SYNC_SETTINGS_CONSUMER_H_
