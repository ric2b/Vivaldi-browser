// Copyright 2022 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_NTP_CELLS_VIVALDI_SPEED_DIAL_FOLDER_REGULAR_CELL_H_
#define IOS_UI_NTP_CELLS_VIVALDI_SPEED_DIAL_FOLDER_REGULAR_CELL_H_

#import <UIKit/UIKit.h>

#import "ios/ui/ntp/vivaldi_speed_dial_item.h"
#import "ios/ui/settings/start_page/layout_settings/vivaldi_start_page_layout_style.h"

// The cell that renders the speed dial folder item for all layout except 'List'
@interface VivaldiSpeedDialFolderRegularCell : UICollectionViewCell

// INITIALIZER
- (instancetype)initWithFrame:(CGRect)rect;

// SETTERS
- (void)configureCellWith:(VivaldiSpeedDialItem*)item
                   addNew:(bool)addNew
              layoutStyle:(VivaldiStartPageLayoutStyle)style;

@end

#endif  // IOS_UI_NTP_CELLS_VIVALDI_SPEED_DIAL_FOLDER_REGULAR_CELL_H_
