// Copyright 2022 Vivaldi Technologies AS. All rights reserved.

#ifndef IOS_UI_NOTES_CELLS_NOTE_PARENT_FOLDER_ITEM_H_
#define IOS_UI_NOTES_CELLS_NOTE_PARENT_FOLDER_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"

// Item to display the name of the parent folder of a note node.
// Used in the folder editor view
@interface NoteParentFolderItem : TableViewItem

// The title of the note folder it represents.
@property(nonatomic, copy) NSString* title;

@end

// Cell class associated to NoteParentFolderItem.
@interface NoteParentFolderCell : TableViewCell

// Label that displays the item's title.
@property(nonatomic, readonly, strong) UILabel* parentFolderNameLabel;

@end

#endif  // IOS_UI_NOTES_CELLS_NOTE_PARENT_FOLDER_ITEM_H_
