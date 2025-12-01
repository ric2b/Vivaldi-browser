// Copyright 2025 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_SETTINGS_SYNC_CELLS_VIVALDI_TABLE_VIEW_TEXT_ITEM_H_
#define IOS_UI_SETTINGS_SYNC_CELLS_VIVALDI_TABLE_VIEW_TEXT_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/cells/legacy_table_view_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"

// TableViewTextItem contains the model data for a TableViewTextCell.
@interface VivaldiTableViewTextItem : TableViewItem

// Text Alignment for the cell's textLabel. Default is NSTextAlignmentNatural.
@property(nonatomic, assign) NSTextAlignment textAlignment;

// UIColor for the cell's textLabel. Default is
// [UIColor colorNamed:kTextPrimaryColor]. ChromeTableViewStyler's
// `cellTitleColor` takes precedence over the default color, but not over
// `textColor`.
@property(nonatomic, strong) UIColor* textColor;
@property(nonatomic, strong) UIColor* backgroundColor;

@property(nonatomic, copy) NSString* text;

// Sets the font for the `text`. Default preferredFontForTextStyle is
// `UIFontTextStyleBody`.
@property(nonatomic, strong) UIFont* textFont;

// Whether this item is enabled. If it is not enabled, the corresponding cell
// has its user interaction disabled. Enabled by default.
@property(nonatomic, assign, getter=isEnabled) BOOL enabled;

@end

// LegacyTableViewCell that displays a text label.
@interface VivaldiTableViewTextCell : LegacyTableViewCell

// The text to display.
@property(nonatomic, readonly, strong) UILabel* textLabel;

@end

#endif  // IOS_UI_SETTINGS_SYNC_CELLS_VIVALDI_TABLE_VIEW_TEXT_ITEM_H_
