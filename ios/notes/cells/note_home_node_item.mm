// Copyright 2022 Vivaldi Technologies AS. All rights reserved.

#import "ios/notes/cells/note_home_node_item.h"

#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"

#include "notes/note_node.h"

#import "ios/chrome/browser/ui/table_view/cells/table_view_url_item.h"
#import "ios/notes/note_utils_ios.h"
#import "ios/notes/cells/note_folder_item.h"
#import "ios/notes/cells/table_view_note_cell.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation NoteHomeNodeItem
@synthesize noteNode = _noteNode;

- (instancetype)initWithType:(NSInteger)type
                noteNode:(const vivaldi::NoteNode*)node {
  if ((self = [super initWithType:type])) {
    if (node->is_folder()) {
      self.cellClass = [TableViewNoteFolderCell class];
    } else {
      self.cellClass = [TableViewNoteCell class];
    }
    _noteNode = node;
  }
  return self;
}

- (void)configureCell:(TableViewCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];
  if (_noteNode->is_folder()) {
    TableViewNoteFolderCell* noteCell =
        base::mac::ObjCCastStrict<TableViewNoteFolderCell>(cell);
    noteCell.folderTitleTextField.text =
        note_utils_ios::TitleForNoteNode(_noteNode);
    noteCell.folderImageView.image =
        [UIImage imageNamed:@"bookmark_blue_folder"];
    noteCell.noteAccessoryType =
        TableViewNoteFolderAccessoryTypeDisclosureIndicator;
    noteCell.accessibilityIdentifier =
        note_utils_ios::TitleForNoteNode(_noteNode);
    noteCell.accessibilityTraits |= UIAccessibilityTraitButton;
  } else {
    TableViewNoteCell* noteCell =
        base::mac::ObjCCastStrict<TableViewNoteCell>(cell);
    noteCell.titleTextField.text =
        note_utils_ios::TitleForNoteNode(_noteNode);
    noteCell.imageView.image =
          [UIImage imageNamed:@"panel_notes"];
    noteCell.accessibilityTraits |= UIAccessibilityTraitButton;
  }
}

@end
