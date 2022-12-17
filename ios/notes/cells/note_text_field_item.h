// Copyright 2022 Vivaldi Technologies AS. All rights reserved.

#ifndef IOS_NOTES_CELLS_NOTE_TEXT_FIELD_ITEM_H_
#define IOS_NOTES_CELLS_NOTE_TEXT_FIELD_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/table_view/cells/table_view_item.h"

@class NoteTextFieldItem;

// Delegates the cell's text field's events.
@protocol NoteTextFieldItemDelegate<UITextFieldDelegate>

// Called when the |text| of the item was changed via the textfield. The item's
// |text| is up-to-date when this is called.
- (void)textDidChangeForItem:(NoteTextFieldItem*)item;

@end

@interface NoteTextFieldItem : TableViewItem

// The text field content.
@property(nonatomic, copy) NSString* text;

// The text field placeholder.
@property(nonatomic, copy) NSString* placeholder;

// Receives the text field events.
@property(nonatomic, weak) id<NoteTextFieldItemDelegate> delegate;

@end

@interface NoteTextFieldCell : TableViewCell

// Label to display the type of content |self.textField| is displaying.
@property(nonatomic, strong) UILabel* titleLabel;

// Text field to display the title of the note node.
@property(nonatomic, strong) UITextField* textField;

// Returns the appropriate text color to use for the given |editing| state.
+ (UIColor*)textColorForEditing:(BOOL)editing;

@end

#endif  // IOS_NOTES_CELLS_NOTE_TEXT_FIELD_ITEM_H_
