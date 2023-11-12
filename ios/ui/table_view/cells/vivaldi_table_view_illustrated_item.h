// Copyright 2022 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_TABLE_VIEW_CELLS_VIVALDI_TABLE_VIEW_ILLUSTRATED_ITEM_H_
#define IOS_UI_TABLE_VIEW_CELLS_VIVALDI_TABLE_VIEW_ILLUSTRATED_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"

@interface VivaldiTableViewIllustratedItem : TableViewItem

@property(nonatomic, strong) UIImage* image;
@property(nonatomic, copy) NSString* title;
@property(nonatomic, copy) NSAttributedString* subtitle;
@end

@interface VivaldiTableViewIllustratedCell : TableViewCell

@property(nonatomic, readonly, strong) UIImageView* illustratedImageView;
@property(nonatomic, readonly, strong) UILabel* titleLabel;
@property(nonatomic, strong) UITextView* subtitleLabel;

@end

#endif  // IOS_UI_TABLE_VIEW_CELLS_VIVALDI_TABLE_VIEW_ILLUSTRATED_ITEM_H_
