// Copyright 2022 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_TABLE_VIEW_CELLS_VIVALDI_TABLE_VIEW_SYNC_USER_INFO_ITEM_H_
#define IOS_UI_TABLE_VIEW_CELLS_VIVALDI_TABLE_VIEW_SYNC_USER_INFO_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_header_footer_item.h"

@interface VivaldiTableViewSyncUserInfoItem : TableViewHeaderFooterItem
@property(nonatomic, copy) NSString* userName;
@property(nonatomic, copy) NSString* userEmail;
@property(nonatomic, copy) UIImage* userAvatar;

@end

@interface VivaldiTableViewSyncUserInfoView : UITableViewHeaderFooterView

@property(nonatomic, readonly, strong) UILabel* userNameLabel;
@property(nonatomic, readonly, strong) UILabel* userEmailLabel;

- (void)setUserAvatar:(UIImage*)image;

@end

#endif  // IOS_UI_TABLE_VIEW_CELLS_VIVALDI_TABLE_VIEW_SYNC_USER_INFO_ITEM_H_
