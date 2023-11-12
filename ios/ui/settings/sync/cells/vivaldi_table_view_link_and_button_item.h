// Copyright 2023 Vivaldi Technologies. All rights reserved

#ifndef IOS_UI_SETTINGS_SYNC_CELLS_VIVALDI_TABLE_VIEW_LINK_AND_BUTTON_ITEM_H_
#define IOS_UI_SETTINGS_SYNC_CELLS_VIVALDI_TABLE_VIEW_LINK_AND_BUTTON_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/table_view/cells/table_view_cell.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_item.h"

// VivaldiTableViewLinkAndButtonItem contains the model for
// VivaldiTableViewLinkAndButtonCell.
@interface VivaldiTableViewLinkAndButtonItem : TableViewItem

// Link being displayed next to the button.
@property(nonatomic, readwrite, strong) NSAttributedString* linkText;

// Text being displayed above the button alignment.
@property(nonatomic, readwrite, assign) NSTextAlignment textAlignment;

// Text for cell button.
@property(nonatomic, readwrite, strong) NSString* buttonText;

// Button text color.
@property(nonatomic, strong) UIColor* buttonTextColor;

// Button background color. Default is custom blue color.
@property(nonatomic, strong) UIColor* buttonBackgroundColor;

// If yes, adds a 50% alpha to the background in disabled state.
// Otherwise, colors in disabled state are the same as in enabled
// state and it is the responsibility of the owner to update color
// before calling `configureCell:withStyler:` (default YES).
@property(nonatomic, assign) BOOL dimBackgroundWhenDisabled;

// Accessibility identifier that will assigned to the button.
@property(nonatomic, strong) NSString* buttonAccessibilityIdentifier;

// Whether the Item's button should be enabled or not. Button is enabled by
// default.
@property(nonatomic, assign, getter=isEnabled) BOOL enabled;

// If YES the item's button width will expand to match the cell's. If NO the
// button will maintain its intrinsic size based on its title. NO by default.
@property(nonatomic, assign) BOOL disableButtonIntrinsicWidth;

@end

// VivaldiTableViewLinkAndButtonCell contains a textLabel and a UIbutton
// laid out horizontally in a stackview and centered.
@interface VivaldiTableViewLinkAndButtonCell : TableViewCell

// Cell text information.
@property(nonatomic, strong) UITextView* label;

// Action button. Note: Set action method in the TableView datasource method.
@property(nonatomic, strong) UIButton* button;

// Enables spacing between items. If there's only one item (e.g. just a button),
// disable the spacing or an extra top padding will be added.
- (void)enableItemSpacing:(BOOL)enable;

// If `disabled` is YES the button's width will expand to match the cell's
// container. If NO, the button will maintain its intrinsic size based on its
// title.
- (void)disableButtonIntrinsicWidth:(BOOL)disable;

@end

#endif  // IOS_UI_SETTINGS_SYNC_CELLS_VIVALDI_TABLE_VIEW_LINK_AND_BUTTON_ITEM_H_
