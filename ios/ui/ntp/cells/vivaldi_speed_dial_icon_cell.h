// Copyright 2025 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_NTP_CELLS_VIVALDI_SPEED_DIAL_ICON_CELL_H_
#define IOS_UI_NTP_CELLS_VIVALDI_SPEED_DIAL_ICON_CELL_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "ios/ui/ntp/vivaldi_speed_dial_item.h"

// The cell that renders the speed dial items for layout style 'Icon'
@interface VivaldiSpeedDialIconCell : UICollectionViewCell

// INITIALIZER
- (instancetype)initWithFrame:(CGRect)rect;

// SETTERS
- (void)configureCellWith:(const VivaldiSpeedDialItem*)item
                 isTablet:(BOOL)isTablet
                 isFolder:(BOOL)isFolder
                wallpaper:(UIImage*)wallpaper;
- (void)configureCellWithAttributes:(const FaviconAttributes*)attributes
                               item:(VivaldiSpeedDialItem*)item;
- (void)configurePreviewForDevice:(BOOL)isTablet;

@end

#endif  // IOS_UI_NTP_CELLS_VIVALDI_SPEED_DIAL_ICON_CELL_H_
