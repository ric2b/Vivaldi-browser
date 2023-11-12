// Copyright 2022 Vivaldi Technologies. All rights reserved.

#import "ios/ui/table_view/cells/vivaldi_table_view_segmented_control_item.h"

#import "base/apple/foundation_util.h"
#import "ios/ui/helpers/vivaldi_uiview_layout_helper.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kPadding = 9.0;
}

@implementation VivaldiTableViewSegmentedControlItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [VivaldiTableViewSegmentedControlCell class];
  }
  return self;
}

- (void)configureCell:(TableViewCell*)tableCell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:tableCell withStyler:styler];
  VivaldiTableViewSegmentedControlCell* cell =
      base::apple::ObjCCastStrict<VivaldiTableViewSegmentedControlCell>(tableCell);
  [cell setSelectionStyle:UITableViewCellSelectionStyleNone];

  [cell initWithLabels:self.labels];
  [cell setSelectedItem:self.selectedItem];
}

@end


@interface VivaldiTableViewSegmentedControlCell ()

@end

@implementation VivaldiTableViewSegmentedControlCell

@synthesize segmentedControl = _segmentedControl;

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  return self;
}

-(void)initWithLabels:(NSArray*)labels {
  if (!self.segmentedControl) {
    self.segmentedControl = [[UISegmentedControl alloc] initWithItems:labels];
    [self.contentView addSubview:self.segmentedControl];

    // Layout
    [self.segmentedControl fillSuperviewWithPadding:
      UIEdgeInsetsMake(kPadding, kPadding, kPadding, kPadding)];
  }
}

-(void)setSelectedItem:(NSInteger)selectedItem {
  if (!self.segmentedControl) {
    return;
  }
  self.segmentedControl.selectedSegmentIndex = selectedItem;
}

@end
