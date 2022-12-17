// Copyright 2022 Vivaldi Technologies AS. All rights reserved.

#ifndef IOS_NOTES_CELLS_TABLE_VIEW_NOTE_CELL_H_
#define IOS_NOTES_CELLS_TABLE_VIEW_NOTE_CELL_H_

#import "ios/chrome/browser/ui/table_view/cells/table_view_item.h"
#import "ios/notes/cells/note_table_cell_title_editing.h"

typedef NS_ENUM(NSInteger, NoteStyle) {
  NoteStyleEntry,
  NoteStyleNew,
};

typedef NS_ENUM(NSInteger, TableViewNoteAccessoryType) {
  TableViewNoteAccessoryTypeNone,
};

// TableViewCell that displays NoteItem data.
// Used in note home table view
@interface TableViewNoteCell
    : TableViewCell <NoteTableCellTitleEditing>

// The leading constraint used to set the cell's leading indentation. The
// default indentationLevel property doesn't affect any custom Cell subviews,
// changing |indentationConstraint| constant property will.
@property(nonatomic, strong, readonly)
    NSLayoutConstraint* indentationConstraint;
// The folder image displayed by this cell.
@property(nonatomic, strong) UIImageView* imageView;
// The folder title displayed by this cell.
@property(nonatomic, strong) UITextField* titleTextField;
// Accessory Type.
@property(nonatomic, assign)
    TableViewNoteAccessoryType noteAccessoryType;
@end

#endif  // IOS_NOTES_CELLS_TABLE_VIEW_NOTE_CELL_H_
