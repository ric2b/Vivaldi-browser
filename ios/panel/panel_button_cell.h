// Copyright 2022 Vivaldi Technologies. All rights reserved.

#ifndef IOS_PANEL_PANEL_BUTTON_CELL_H_
#define IOS_PANEL_PANEL_BUTTON_CELL_H_

#import <UIKit/UIKit.h>

// The cell holds an icon for the Panel page
@interface PanelButtonCell : UICollectionViewCell

// INITIALIZER
- (instancetype)initWithFrame:(CGRect)rect;

// SETTERS
- (void)configureCellWithIndex:(NSInteger)index
                   highlighted:(BOOL)highlighted;

@end

#endif  // IOS_PANEL_PANEL_BUTTON_CELL_H_
