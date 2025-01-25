// Copyright 2024 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_NTP_VIVALDI_SPEED_DIAL_CONTAINER_VIEW_FLOW_LAYOUT_H_
#define IOS_UI_NTP_VIVALDI_SPEED_DIAL_CONTAINER_VIEW_FLOW_LAYOUT_H_

#import <UIKit/UIKit.h>

#import "ios/ui/settings/start_page/layout_settings/vivaldi_start_page_layout_column.h"
#import "ios/ui/settings/start_page/layout_settings/vivaldi_start_page_layout_state.h"
#import "ios/ui/settings/start_page/layout_settings/vivaldi_start_page_layout_style.h"

@interface VivaldiSpeedDialViewContainerViewFlowLayout:
    UICollectionViewFlowLayout

@property (assign, nonatomic) VivaldiStartPageLayoutStyle layoutStyle;
@property (assign, nonatomic) VivaldiStartPageLayoutColumn numberOfColumns;
@property (assign, nonatomic) VivaldiStartPageLayoutState layoutState;

// Boolean to keep track when to show tablet layout for the items. Item size
// for tablet layout differs from the one from Phone layout.
// Tablet layout is visible when iPad is on full screen or 2/3 on SplitView.
@property (assign, nonatomic) BOOL shouldShowTabletLayout;
// When only one group is visible on start page, either top sites or a single
// SD folder/group. We remove top inset for the collection view in that case.
@property (assign, nonatomic) BOOL topToolbarHidden;

// Maximum number of rows the flowlayout should have. Minimum bound is
// determined based on the actual number of items of the collection view.
@property (assign, nonatomic) NSInteger maxNumberOfRows;

/// Update collection view height after items loaded.
- (void)adjustCollectionViewHeight;
/// Height needed to show contents based on actual numberOfRows
/// and maxNumberOfRows.
- (CGFloat)heightNeededForContents;

@end

#endif  // IOS_UI_NTP_VIVALDI_SPEED_DIAL_CONTAINER_VIEW_FLOW_LAYOUT_H_
