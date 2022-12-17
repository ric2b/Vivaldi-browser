// Copyright 2022 Vivaldi Technologies AS. All rights reserved.

#ifndef IOS_NOTES_CELLS_NOTE_TEXT_VIEW_ITEM_H_
#define IOS_NOTES_CELLS_NOTE_TEXT_VIEW_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/table_view/cells/table_view_item.h"

@class NoteTextViewItem;

// Delegates the cell's text view's events.
@protocol NoteTextViewItemDelegate<UITextViewDelegate>

// Called when the |text| of the item was changed via the textfield. The item's
// |text| is up-to-date when this is called.
- (void)textDidChangeForTextViewItem:(NoteTextViewItem*)item;

@end

// Item that shows note text in edit view
@interface NoteTextViewItem<UITextViewDelegate> : TableViewItem

// The text field content.
@property(nonatomic, copy) NSString* text;

// The text field placeholder.
@property(nonatomic, copy) NSString* placeholder;

// Receives the text view events.
@property(nonatomic, weak) id<UITextViewDelegate> delegate;

@end

@interface NoteTextViewCell : TableViewCell

// Text field to display the title or the title of the note node.
@property(nonatomic, strong) UITextView* textView;

// Returns the appropriate text color to use for the given |editing| state.
+ (UIColor*)textColorForEditing:(BOOL)editing;

@end

#endif  // IOS_NOTES_CELLS_NOTE_TEXT_FIELD_ITEM_LONG_H_
