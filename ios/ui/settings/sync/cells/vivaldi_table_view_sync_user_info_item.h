// Copyright 2022 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_TABLE_VIEW_CELLS_VIVALDI_TABLE_VIEW_SYNC_USER_INFO_ITEM_H_
#define IOS_UI_TABLE_VIEW_CELLS_VIVALDI_TABLE_VIEW_SYNC_USER_INFO_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_header_footer_item.h"

@protocol VivaldiTableViewSyncUserInfoViewDelegate <NSObject>
- (void)didTapSessionEditButtonWithCurrentSession:(NSString*)sessionName;
@end

@interface VivaldiTableViewSyncUserInfoItem : TableViewHeaderFooterItem
@property(nonatomic, copy) NSString* userName;
@property(nonatomic, copy) NSString* sessionName;
@property(nonatomic, copy) UIImage* userAvatar;
@property(nonatomic, copy) UIImage* supporterBadge;

@end

@interface VivaldiTableViewSyncUserInfoView : UITableViewHeaderFooterView

@property(nonatomic, readonly, strong) UILabel* userNameLabel;
@property(nonatomic, readonly, strong) UILabel* sessionNameLabel;

@property (nonatomic, weak)
    id<VivaldiTableViewSyncUserInfoViewDelegate> delegate;

- (void)setUserAvatar:(UIImage*)image;
- (void)setBadgeImage:(UIImage*)badgeImage;

@end

#endif  // IOS_UI_TABLE_VIEW_CELLS_VIVALDI_TABLE_VIEW_SYNC_USER_INFO_ITEM_H_
