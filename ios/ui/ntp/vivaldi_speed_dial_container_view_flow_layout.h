// Copyright 2024 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_NTP_VIVALDI_SPEED_DIAL_CONTAINER_VIEW_FLOW_LAYOUT_H_
#define IOS_UI_NTP_VIVALDI_SPEED_DIAL_CONTAINER_VIEW_FLOW_LAYOUT_H_

#import <UIKit/UIKit.h>

#import "ios/ui/settings/start_page/layout_settings/vivaldi_start_page_layout_column.h"
#import "ios/ui/settings/start_page/layout_settings/vivaldi_start_page_layout_style.h"

@interface VivaldiSpeedDialViewContainerViewFlowLayout:
    UICollectionViewFlowLayout

@property (assign, nonatomic) VivaldiStartPageLayoutStyle layoutStyle;
@property (assign, nonatomic) VivaldiStartPageLayoutColumn numberOfColumns;
@property (assign, nonatomic) BOOL isPreview;

@end

#endif  // IOS_UI_NTP_VIVALDI_SPEED_DIAL_CONTAINER_VIEW_FLOW_LAYOUT_H_
