// Copyright 2022 Vivaldi Technologies AS. All rights reserved.

#ifndef IOS_UI_AD_TRACKER_BLOCKER_CELLS_VIVALDI_ATB_SETTING_ITEM_H_
#define IOS_UI_AD_TRACKER_BLOCKER_CELLS_VIVALDI_ATB_SETTING_ITEM_H_

#import "ios/chrome/browser/ui/table_view/cells/table_view_item.h"
#import "ios/ui/ad_tracker_blocker/vivaldi_atb_item.h"

// VivaldiATBGlobalSettingItem provides data for a table view row that displays
// a single option of tracker blocker setting
@interface VivaldiATBSettingItem : TableViewItem

- (instancetype)initWithType:(NSInteger)type;

@property(nonatomic, strong) VivaldiATBItem* item;
@property(nonatomic, assign) ATBSettingOption userPreferredOption;
@property(nonatomic, assign) ATBSettingOption globalDefaultOption;

@end

// TableViewCell that displays Ad and tracker setting option.
@interface VivaldiATBSettingItemCell
    : TableViewCell

- (void)configurWithItem:(VivaldiATBItem*)item
     userPreferredOption:(ATBSettingOption)userPreferred
     globalDefaultOption:(ATBSettingOption)globalDefault;

@end

#endif  // IOS_UI_AD_TRACKER_BLOCKER_CELLS_VIVALDI_ATB_SETTING_ITEM_H_
