// Copyright 2024 Vivaldi Technologies. All rights reserved

#ifndef IOS_UI_SETTINGS_SYNC_CELLS_TABLE_VIEW_ATTRIBUTED_TEXT_VIEW_ITEM_H_
#define IOS_UI_SETTINGS_SYNC_CELLS_TABLE_VIEW_ATTRIBUTED_TEXT_VIEW_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"


@interface VivaldiTableViewAttributedTextViewItem : TableViewItem

@property(nonatomic, readwrite, strong) NSAttributedString* text;

@end

// TableViewCell that displays a text label that might contain a link.
@interface VivaldiTableViewAttributedTextViewCell : TableViewCell

@property(nonatomic, readonly, strong) UITextView* textView;

@end

#endif  // IOS_UI_SETTINGS_SYNC_CELLS_TABLE_VIEW_ATTRIBUTED_TEXT_VIEW_ITEM_H_
