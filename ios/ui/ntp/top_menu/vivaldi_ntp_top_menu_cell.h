// Copyright 2022 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_NTP_TOP_MENU_VIVALDI_NTP_TOP_MENU_CELL_H_
#define IOS_UI_NTP_TOP_MENU_VIVALDI_NTP_TOP_MENU_CELL_H_

#import <UIKit/UIKit.h>

#import "ios/ui/ntp/vivaldi_speed_dial_item.h"

// The cell holds the title of the menu item and the selection indicator
@interface VivaldiNTPTopMenuCell : UICollectionViewCell

// INITIALIZER
- (instancetype)initWithFrame:(CGRect)rect;

// SETTERS
- (void)configureCellWithItem:(VivaldiSpeedDialItem*)item;

@end

#endif  // IOS_UI_NTP_TOP_MENU_VIVALDI_NTP_TOP_MENU_CELL_H_
