// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/send_tab_to_self/send_tab_to_self_image_detail_text_item.h"

#import "base/check.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/settings/cells/settings_image_detail_text_cell.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"

// Vivaldi
#import "app/vivaldi_apptools.h"
#import "ios/chrome/browser/ui/send_tab_to_self/send_tab_to_self_image_detail_text_cell.h"
#import "ios/ui/toolbar/vivaldi_toolbar_constants.h"

using vivaldi::IsVivaldiRunning;
// End Vivaldi

@implementation SendTabToSelfImageDetailTextItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    if (IsVivaldiRunning()) {
      self.cellClass = [SendTabToSelfImageDetailTextCell class];
    } else {
    self.cellClass = [SettingsImageDetailTextCell class];
    } // End Vivaldi
  }
  return self;
}

#if defined(VIVALDI_BUILD)
- (void)configureCell:(SendTabToSelfImageDetailTextCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
#else
- (void)configureCell:(SettingsImageDetailTextCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
#endif // End Vivaldi
  [super configureCell:cell withStyler:styler];
  cell.textLabel.text = self.text;
  cell.detailTextLabel.text = self.detailText;

  if (IsVivaldiRunning()) {
    [cell setImageViewTintColor:[UIColor colorNamed:vToolbarButtonColor]];
  } else {
  [cell setImageViewTintColor:[UIColor colorNamed:kGrey400Color]];
  } // End Vivaldi

  DCHECK(self.iconImage);
  cell.image = self.iconImage;
  if (self.selected) {
    cell.accessoryType = UITableViewCellAccessoryCheckmark;
  } else {
    cell.accessoryType = UITableViewCellAccessoryNone;
  }
}

@end
