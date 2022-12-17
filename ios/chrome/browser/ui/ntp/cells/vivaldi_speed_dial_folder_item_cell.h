// Copyright 2022 Vivaldi Technologies. All rights reserved.

#ifndef IOS_CHROME_BROWSER_UI_VIVALDI_SPEED_DIAL_FOLDER_ITEM_CELL_H_
#define IOS_CHROME_BROWSER_UI_VIVALDI_SPEED_DIAL_FOLDER_ITEM_CELL_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/ntp/vivaldi_speed_dial_item.h"

// The cell that renders the speed dial folder items
@interface VivaldiSpeedDialFolderItemCell : UICollectionViewCell

// INITIALIZER
- (instancetype)initWithFrame:(CGRect)rect;

// SETTERS
- (void)configureCellWith:(VivaldiSpeedDialItem*)item
                   addNew:(bool)addNew;

@end

#endif  // IOS_CHROME_BROWSER_UI_VIVALDI_SPEED_DIAL_FOLDER_ITEM_CELL_H_
