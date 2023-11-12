// Copyright 2022 Vivaldi Technologies AS. All rights reserved.

#ifndef IOS_UI_NOTES_CELLS_NOTE_TABLE_CELL_TITLE_EDITING_H_
#define IOS_UI_NOTES_CELLS_NOTE_TABLE_CELL_TITLE_EDITING_H_

@protocol NoteTableCellTitleEditDelegate;

// Cell title editing public interface.
@protocol NoteTableCellTitleEditing

// Receives the text field events.
@property(nonatomic, weak) id<NoteTableCellTitleEditDelegate> textDelegate;
// Start editing the |folderTitleTextField| of this cell.
- (void)startEdit;
// Stop editing the title's text of this cell. This will call textDidChangeTo:
// on |textDelegate| with the cell's title text value at the moment.
- (void)stopEdit;

@end

#endif  // IOS_UI_NOTES_CELLS_NOTE_TABLE_CELL_TITLE_EDITING_H_
