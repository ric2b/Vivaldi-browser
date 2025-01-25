// Copyright 2022 Vivaldi Technologies. All rights reserved.

#import "ios/ui/table_view/cells/vivaldi_table_view_text_button_item.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Alpha value for the disabled action button.
const CGFloat kDisabledButtonAlpha = 0.5;
}  // namespace

@implementation VivaldiTableViewTextButtonItem

- (instancetype)initWithType:(NSInteger)type {
  return [super initWithType:type];
}

- (void)configureCell:(TableViewCell*)tableCell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:tableCell withStyler:styler];
  TableViewTextButtonCell* cell =
      base::apple::ObjCCastStrict<TableViewTextButtonCell>(tableCell);

  [cell.button setTitleColor:[[UIColor colorNamed:kBlueColor]
      colorWithAlphaComponent:kDisabledButtonAlpha]
                    forState:UIControlStateDisabled];
  cell.button.backgroundColor =
      [UIColor colorNamed:kBackgroundColor];

  [NSLayoutConstraint activateConstraints:@[
    [cell.button.leadingAnchor
        constraintEqualToAnchor:cell.contentView.leadingAnchor],
    [cell.button.topAnchor
        constraintEqualToAnchor:cell.contentView.topAnchor],
    [cell.button.bottomAnchor
        constraintEqualToAnchor:cell.contentView.bottomAnchor],
    [cell.button.trailingAnchor
        constraintEqualToAnchor:cell.contentView.trailingAnchor]
  ]];
}

@end
