// Copyright 2022 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_TABLE_VIEW_CELLS_VIVALDI_TABLE_VIEW_SYNC_STATUS_ITEM_H_
#define IOS_UI_TABLE_VIEW_CELLS_VIVALDI_TABLE_VIEW_SYNC_STATUS_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"

@interface VivaldiTableViewSyncStatusItem : TableViewItem
// String containing the last time data was synchronized.
@property(nonatomic, copy) NSString* lastSyncDateString;
// Indicates if sync is connected and active
@property(nonatomic) NSString* statusText;
@property(nonatomic) UIColor* statusBackgroundColor;
@end

@interface VivaldiTableViewSyncStatusCell : TableViewCell

// Label displaying the last sync data
@property(nonatomic, readonly, strong) UILabel* lastSyncDateLabel;
// Label displaying the sync status
@property(nonatomic, readonly, strong) UILabel* syncActiveLabel;
@property(nonatomic, readonly, strong) UIView* syncStatusView;

- (void)updateLabelCornerRadius;

@end

#endif  // IOS_UI_TABLE_VIEW_CELLS_VIVALDI_TABLE_VIEW_SYNC_STATUS_ITEM_H_
