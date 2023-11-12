// Copyright 2022 Vivaldi Technologies AS. All rights reserved.

#import "ios/notes/cells/note_home_node_item.h"

#import "base/mac/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/ui/bookmarks/vivaldi_bookmarks_constants.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_url_item.h"
#import "ios/notes/cells/note_folder_item.h"
#import "ios/notes/cells/table_view_note_cell.h"
#import "ios/notes/note_utils_ios.h"
#import "notes/note_node.h"
#import "ui/base/l10n/l10n_util.h"
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
        base::mac::ObjCCastStrict<TableViewNoteFolderCell>(cell);
    noteCell.folderTitleTextField.text =
        note_utils_ios::TitleForNoteNode(_noteNode);
    noteCell.folderImageView.image =
        [UIImage imageNamed:vBookmarksFolderIcon];
    noteCell.noteAccessoryType =
        TableViewNoteFolderAccessoryTypeDisclosureIndicator;
    noteCell.accessibilityIdentifier =
        note_utils_ios::TitleForNoteNode(_noteNode);
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
    base::mac::ObjCCastStrict<TableViewNoteCell>(cell);
    [noteCell
        configureNoteWithTitle:note_utils_ios::TitleForNoteNode(_noteNode)
                     createdAt:note_utils_ios::createdAtForNoteNode(_noteNode)];
    noteCell.accessibilityLabel = [noteCell accessibilityLabelString];
    noteCell.accessibilityTraits |= UIAccessibilityTraitButton;
  }
}

@end
