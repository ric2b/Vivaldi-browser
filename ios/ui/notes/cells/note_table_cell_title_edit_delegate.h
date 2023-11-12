// Copyright 2022 Vivaldi Technologies AS. All rights reserved.

#ifndef IOS_UI_NOTES_CELLS_NOTE_TABLE_CELL_TITLE_EDIT_DELEGATE_H_
#define IOS_UI_NOTES_CELLS_NOTE_TABLE_CELL_TITLE_EDIT_DELEGATE_H_

// Delegates the cell's text field's event.
@protocol NoteTableCellTitleEditDelegate

// Called when the |titleText| of the cell was changed.
- (void)textDidChangeTo:(NSString*)newName;

@end

#endif  // IOS_UI_NOTES_CELLS_NOTE_TABLE_CELL_TITLE_EDIT_DELEGATE_H_
