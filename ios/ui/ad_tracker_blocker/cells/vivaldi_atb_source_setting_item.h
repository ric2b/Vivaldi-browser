// Copyright 2023 Vivaldi Technologies AS. All rights reserved.

#ifndef IOS_UI_AD_TRACKER_BLOCKER_CELLS_VIVALDI_ATB_SOURCE_SETTING_ITEM_H_
#define IOS_UI_AD_TRACKER_BLOCKER_CELLS_VIVALDI_ATB_SOURCE_SETTING_ITEM_H_

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_item.h"
#import "ios/ui/ad_tracker_blocker/vivaldi_atb_source_item.h"

// VivaldiATBSourceSettingItem provides data for a table view row that displays
// source setting.
@interface VivaldiATBSourceSettingItem : TableViewSwitchItem

- (instancetype)initWithType:(NSInteger)type;

@property(nonatomic, strong) VivaldiATBSourceItem* source;


@end

#endif  // IOS_UI_AD_TRACKER_BLOCKER_CELLS_VIVALDI_ATB_SOURCE_SETTING_ITEM_H_
