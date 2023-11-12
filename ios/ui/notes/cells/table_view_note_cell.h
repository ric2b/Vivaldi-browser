// Copyright 2022 Vivaldi Technologies AS. All rights reserved.

#ifndef IOS_UI_NOTES_CELLS_TABLE_VIEW_NOTE_CELL_H_
#define IOS_UI_NOTES_CELLS_TABLE_VIEW_NOTE_CELL_H_

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"

// TableViewCell that displays NoteItem data.
// Used in note home table view
@interface TableViewNoteCell
    : TableViewCell

// Getter
- (NSString*)accessibilityLabelString;

// Setter
-(void)configureNoteWithTitle:(NSString*)title
                    createdAt:(NSDate*)createdAt;

@end

#endif  // IOS_UI_NOTES_CELLS_TABLE_VIEW_NOTE_CELL_H_
