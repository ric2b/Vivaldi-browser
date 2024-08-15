// Copyright 2022 Vivaldi Technologies AS. All rights reserved.

#import "ios/ui/notes/cells/note_home_node_item.h"

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/notes/note_node.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_url_item.h"
#import "ui/base/l10n/l10n_util.h"
#import "ios/ui/notes/cells/note_folder_item.h"
#import "ios/ui/notes/cells/table_view_note_cell.h"
#import "ios/ui/notes/note_ui_constants.h"
#import "ios/ui/notes/note_utils_ios.h"
#import "vivaldi/ios/grit/vivaldi_ios_native_strings.h"

using l10n_util::GetNSString;

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
        base::apple::ObjCCastStrict<TableViewNoteFolderCell>(cell);
    noteCell.folderTitleTextField.text =
        [self noteTitle];
    if (self.shouldShowTrashIcon) {
      noteCell.folderImageView.image =
          [UIImage imageNamed:vNotesTrashFolderIcon];
    } else {
      noteCell.folderImageView.image =
          [UIImage imageNamed:vNotesFolderIcon];
    }
    noteCell.noteAccessoryType =
        TableViewNoteFolderAccessoryTypeDisclosureIndicator;
    noteCell.accessibilityIdentifier =
        [self noteTitle];
    noteCell.accessibilityTraits |= UIAccessibilityTraitButton;

    // Folder items count
    NSString* notesString =
        [GetNSString(IDS_VIVALDI_TOOLS_MENU_NOTES) lowercaseString];
    NSString* noteString =
        [GetNSString(IDS_VIVALDI_FOLDER_NOTE) lowercaseString];
    int itemsCount = _noteNode->children().size();
    noteCell.folderItemsLabel.text = itemsCount == 0 ?
        GetNSString(IDS_VIVALDI_NOTE_NO_ITEM_COUNT) :
        [NSString stringWithFormat: @"%d%@%@", itemsCount, @" ",
            (itemsCount > 1 ? notesString : noteString)];
  } else {
    TableViewNoteCell* noteCell =
        base::apple::ObjCCastStrict<TableViewNoteCell>(cell);
    [noteCell configureNoteWithTitle:[self noteTitle]
                     createdAt:[self createdAt]];
    noteCell.accessibilityLabel = [noteCell accessibilityLabelString];
    noteCell.accessibilityTraits |= UIAccessibilityTraitButton;
    noteCell.imageView.image = [UIImage imageNamed:vNotesIcon];
  }
}

#pragma mark - Helpers

- (NSString*)noteTitle {
  return note_utils_ios::TitleForNoteNode(_noteNode);
}

- (NSDate*)createdAt {
  return note_utils_ios::createdAtForNoteNode(_noteNode);
}

- (NSDate*)lastModified {
  return note_utils_ios::lastModificationTimeForNoteNode(_noteNode);
}

- (BOOL)isFolder {
  return _noteNode->is_folder();
}

@end
