// Copyright 2024 Vivaldi Technologies. All rights reserved.

#ifndef IOS_CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_IMAGE_DETAIL_TEXT_CELL_H_
#define IOS_CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_IMAGE_DETAIL_TEXT_CELL_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"

// Cell representation for SendTabToSelfImageDetailTextCell.
//  +--------------------------------------------------+
//  |  +-------+                                       |
//  |  | image |   Multiline title                     |
//  |  |       |   Optional multiline detail text      |
//  |  +-------+                                       |
//  +--------------------------------------------------+
@interface SendTabToSelfImageDetailTextCell : TableViewCell

// Cell image.
@property(nonatomic, strong) UIImage* image;

// Cell title.
@property(nonatomic, readonly, strong) UILabel* textLabel;

// Cell subtitle.
@property(nonatomic, readonly, strong) UILabel* detailTextLabel;

// Sets the image view's alpha.
- (void)setImageViewAlpha:(CGFloat)alpha;

// Sets the image view's tint color.
- (void)setImageViewTintColor:(UIColor*)color;

@end

#endif  // IOS_CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_IMAGE_DETAIL_TEXT_CELL_H_
