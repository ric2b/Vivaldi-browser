// Copyright 2022 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_TABLE_VIEW_CELLS_TABLE_VIEW_SEGMENTED_CONTROL_ITEM_H_
#define IOS_UI_TABLE_VIEW_CELLS_TABLE_VIEW_SEGMENTED_CONTROL_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"

@interface VivaldiTableViewSegmentedControlItem : TableViewItem

@property(nonatomic, strong) NSArray* labels;
@property(nonatomic, assign) NSInteger selectedItem;

@end


@interface VivaldiTableViewSegmentedControlCell : TableViewCell

@property(nonatomic, strong) UISegmentedControl* segmentedControl;

-(void)initWithLabels:(NSArray*)labels;
-(void)setSelectedItem:(NSInteger)selectedItem;

@end

#endif  // IOS_UI_TABLE_VIEW_CELLS_TABLE_VIEW_SEGMENTED_CONTROL_ITEM_H_
