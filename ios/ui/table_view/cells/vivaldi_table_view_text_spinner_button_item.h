// Copyright 2024 Vivaldi Technologies. All rights reserved.

#ifndef VIVALDI_IOS_UI_TABLE_VIEW_CELLS_VIVALDI_TABLE_VIEW_TEXT_SPINNER_BUTTON_ITEM_H_
#define VIVALDI_IOS_UI_TABLE_VIEW_CELLS_VIVALDI_TABLE_VIEW_TEXT_SPINNER_BUTTON_ITEM_H_

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_button_item.h"

// Tableview button item but with a spinner inside to show activity indicator
@interface VivaldiTableViewTextSpinnerButtonItem : TableViewTextButtonItem

// Whether the Item's button title should be hidden and activity indicator is
// showing.
@property(nonatomic, assign) BOOL activityInProgress;

@end

// VivaldiTableViewTextSpinnerButtonCell contains an activity indicator
// laid out vertically and centered.
@interface VivaldiTableViewTextSpinnerButtonCell: TableViewTextButtonCell

- (void)setUpViewWithActivityIndicator;
- (void)setActivityIndicatorEnabled:(BOOL)enable;

@end

#endif  // VIVALDI_IOS_UI_TABLE_VIEW_CELLS_VIVALDI_TABLE_VIEW_TEXT_SPINNER_BUTTON_ITEM_H_
