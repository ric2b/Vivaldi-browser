// Copyright 2022 Vivaldi Technologies AS. All rights reserved.

#ifndef IOS_UI_NOTES_CELLS_NOTE_FOLDER_ITEM_H_
#define IOS_UI_NOTES_CELLS_NOTE_FOLDER_ITEM_H_

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"
#import "ios/ui/notes/cells/note_table_cell_title_editing.h"

typedef NS_ENUM(NSInteger, NoteFolderStyle) {
  NoteFolderStyleFolderEntry,
  NoteFolderStyleNewFolder,
};

typedef NS_ENUM(NSInteger, TableViewNoteFolderAccessoryType) {
  TableViewNoteFolderAccessoryTypeNone,
  TableViewNoteFolderAccessoryTypeCheckmark,
  TableViewNoteFolderAccessoryTypeDisclosureIndicator,
};

namespace vivaldi {
class NoteNode;
}

// NoteFolderItem provides data for a table view row that displays a
// single note folder.
@interface NoteFolderItem : TableViewItem

// The Item's designated initializer. If |style| is
// NoteFolderStyle then all other property values will be
// ignored.
- (instancetype)initWithType:(NSInteger)type
                       style:(NoteFolderStyle)style
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithType:(NSInteger)type NS_UNAVAILABLE;

// The item's title.
@property(nonatomic, copy) NSString* title;
// YES if the FolderItem is the current folder.
@property(nonatomic, assign, getter=isCurrentFolder) BOOL currentFolder;
// The item's left indentation level.
@property(nonatomic, assign) NSInteger indentationLevel;

@property(nonatomic, assign) const vivaldi::NoteNode* noteNode;

@end

// TableViewCell that displays NoteFolderItem data.
@interface TableViewNoteFolderCell
    : TableViewCell <NoteTableCellTitleEditing>

// The leading constraint used to set the cell's leading indentation. The
// default indentationLevel property doesn't affect any custom Cell subviews,
// changing |indentationConstraint| constant property will.
@property(nonatomic, strong, readonly)
    NSLayoutConstraint* indentationConstraint;
// The folder image displayed by this cell.
@property(nonatomic, strong) UIImageView* folderImageView;
// The folder title displayed by this cell.
@property(nonatomic, strong) UITextField* folderTitleTextField;
// The folder child count displayed by this cell
@property(nonatomic, strong) UILabel* folderItemsLabel;
// Accessory Type.
@property(nonatomic, assign)
    TableViewNoteFolderAccessoryType noteAccessoryType;
@end

#endif  // IOS_UI_NOTES_CELLS_NOTE_FOLDER_ITEM_H_
