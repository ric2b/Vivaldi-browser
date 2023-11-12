// Copyright 2022 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_NTP_CELLS_VIVALDI_SPEED_DIAL_CONTAINER_CELL_H_
#define IOS_UI_NTP_CELLS_VIVALDI_SPEED_DIAL_CONTAINER_CELL_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/favicon/favicon_loader.h"
#import "ios/ui/ntp/vivaldi_speed_dial_container_delegate.h"
#import "ios/ui/ntp/vivaldi_speed_dial_container_view.h"
#import "ios/ui/settings/start_page/vivaldi_start_page_layout_style.h"

// The cell to contain the children of speed dial folder
@interface VivaldiSpeedDialContainerCell : UICollectionViewCell

// INITIALIZER
- (instancetype)initWithFrame:(CGRect)rect;

// DELEGATE
@property (nonatomic, weak) id<VivaldiSpeedDialContainerDelegate> delegate;

// SETTERS
- (void)configureWith:(NSArray*)speedDials
               parent:(VivaldiSpeedDialItem*)parent
        faviconLoader:(FaviconLoader*)faviconLoader
          layoutStyle:(VivaldiStartPageLayoutStyle)style;
- (void)reloadLayoutWithStyle:(VivaldiStartPageLayoutStyle)style;
- (void)setCurrentPage:(NSInteger)page;
@end

#endif  // IOS_UI_NTP_CELLS_VIVALDI_SPEED_DIAL_CONTAINER_CELL_H_
