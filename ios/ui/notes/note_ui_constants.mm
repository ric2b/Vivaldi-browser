// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#import "ios/ui/notes/note_ui_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSString* const kNoteHomeViewContainerIdentifier =
    @"kNoteHomeViewContainerIdentifier";
NSString* const kNoteAddEditViewContainerIdentifier =
    @"kNoteAddEditViewContainerIdentifier";
NSString* const kNoteFolderEditViewContainerIdentifier =
    @"kNoteFolderEditViewContainerIdentifier";
NSString* const kNoteFolderCreateViewContainerIdentifier =
    @"kNoteFolderCreateViewContainerIdentifier";
NSString* const kNoteFolderPickerViewContainerIdentifier =
    @"kNoteFolderPickerViewContainerIdentifier";

NSString* const kNoteHomeTableViewIdentifier =
    @"kNoteHomeTableViewIdentifier";
NSString* const kNoteHomeContextMenuIdentifier =
    @"kNoteHomeContextMenuIdentifier";

NSString* const kNoteNavigationBarIdentifier =
    @"kNoteNavigationBarIdentifier";
NSString* const kNoteHomeNavigationBarDoneButtonIdentifier =
    @"kNoteHomeNavigationBarDoneButtonIdentifier";
NSString* const kNoteEditNavigationBarDoneButtonIdentifier =
    @"kNoteEditNavigationBarDoneButtonIdentifier";
NSString* const kNoteFolderEditNavigationBarDoneButtonIdentifier =
    @"kNoteFolderEditNavigationBarDoneButtonIdentifier";

NSString* const kNoteEditDeleteButtonIdentifier =
    @"kNoteEditDeleteButtonIdentifier";
NSString* const kNoteEditMoveButtonIdentifier =
    @"kNoteEditMoveButtonIdentifier";
NSString* const kNoteFolderEditorDeleteButtonIdentifier =
    @"kNoteFolderEditorDeleteButtonIdentifier";
NSString* const kNoteHomeLeadingButtonIdentifier =
    @"kNoteHomeLeadingButtonIdentifier";
NSString* const kNoteHomeCenterButtonIdentifier =
    @"kNoteHomeCenterButtonIdentifier";
NSString* const kNoteHomeTrailingButtonIdentifier =
    @"kNoteHomeTrailingButtonIdentifier";
NSString* const kNoteHomeUIToolbarIdentifier =
    @"kNoteHomeUIToolbarIdentifier";
NSString* const kNoteHomeSearchBarIdentifier =
    @"kNoteHomeSearchBarIdentifier";
NSString* const kNoteHomeSearchScrimIdentifier =
    @"kNoteHomeSearchScrimIdentifier";

const CGFloat kNoteCellViewSpacing = 10.0f;
const CGFloat kNoteCellVerticalInset = 11.0f;
const CGFloat kNoteCellHorizontalLeadingInset = 16.0f;
const CGFloat kNoteCellHorizontalAccessoryViewSpacing = 11.0f;

NSString* const kNoteCreateNewFolderCellIdentifier =
    @"kNoteCreateNewFolderCellIdentifier";

NSString* const kNoteEmptyStateExplanatoryLabelIdentifier =
    @"kNoteEmptyStateExplanatoryLabelIdentifier";

NSString* const kToolsMenuNotesId = @"kToolsMenuNotesId";
NSString* const kToolsMenuAddToNotes = @"kToolsMenuAddToNotes";

#pragma mark - ICONS
// Image name for the note icon.
NSString* vNotesIcon = @"single_note_icon";
// Image name for the note folder icon.
NSString* vNotesFolderIcon = @"vivaldi_bookmarks_folder";
// Image name for trash folder icon
NSString* vNotesTrashFolderIcon = @"vivaldi_trash_folder";
// Image name for the folder selection chevron.
NSString* vNoteFolderSelectionChevron =
  @"vivaldi_bookmark_folder_selection_chevron";
// Image name for the folder selection checkmark.
NSString* vNoteFolderSelectionCheckmark =
  @"vivaldi_bookmark_folder_selection_checkmark";
// Image name for add folder
NSString* vNoteAddFolder =
  @"vivaldi_bookmark_add_new_folder";

#pragma mark - SIZE
// Corner radius for the note body container view
CGFloat vNoteBodyCornerRadius = 6.0;
// Note editor text view height
CGFloat vNoteTextViewHeight = 44.0;
//  Note folder selection header view height
CGFloat vNoteFolderSelectionHeaderViewHeight = 60.0;

#pragma mark - OTHERS
// Maximum number of entries to fetch when searching on notes folder page.
const int vMaxNoteFolderSearchResults = 50;

#pragma mark - MARKDOWN
NSString* vMarkdownLibraryBundleName = @"mobile-markdown-bundle";
NSString* vMarkdownHTMLFilename = @"markdown";