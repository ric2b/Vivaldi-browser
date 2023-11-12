// Copyright 2023 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_TABLE_VIEW_CELLS_VIVALDI_INPUT_ERROR_ITEM_H_
#define IOS_UI_TABLE_VIEW_CELLS_VIVALDI_INPUT_ERROR_ITEM_H_

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"

@class SettingsRootTableViewController;

// Item to display an error when the input validation fails.
@interface VivaldiInputErrorItem : TableViewItem

// The error text. It appears in red.
@property(nonatomic, copy) NSString* text;

@property(nonatomic, strong) UIColor* textColor;

@end

// Cell class associated to VivaldiInputErrorItem.
@interface VivaldiInputErrorCell : TableViewCell

// Label for the error text.
@property(nonatomic, readonly, strong) UILabel* textLabel;

@end

#endif  // IOS_UI_TABLE_VIEW_CELLS_VIVALDI_INPUT_ERROR_ITEM_H_
