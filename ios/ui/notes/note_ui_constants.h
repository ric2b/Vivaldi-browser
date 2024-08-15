// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#ifndef IOS_UI_NOTES_NOTE_UI_CONSTANTS_H_
#define IOS_UI_NOTES_NOTE_UI_CONSTANTS_H_

#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>

#import "chromium/build/build_config.h"

// Note container constants:
// Accessibility identifier of the Note Edit View container.
extern NSString* const kNoteAddEditViewContainerIdentifier;
// Accessibility identifier of the Note Folder Edit View container.
extern NSString* const kNoteFolderEditViewContainerIdentifier;
// Accessibility identifier of the Note Folder Create View container.
extern NSString* const kNoteFolderCreateViewContainerIdentifier;
// Accessibility identifier of the Note Folder Picker View container.
extern NSString* const kNoteFolderPickerViewContainerIdentifier;
// Accessibility identifier of the Note Home TableView.
extern NSString* const kNoteHomeTableViewIdentifier;
// Accessibility identifier of the Note Home context menu.
extern NSString* const kNoteHomeContextMenuIdentifier;

// UINavigationBar accessibility constants:
// Accessibility identifier of the Note navigation bar.
extern NSString* const kNoteNavigationBarIdentifier;
// Accessibility identifier of the NoteHome VC navigation bar done button.
extern NSString* const kNoteHomeNavigationBarDoneButtonIdentifier;
// Accessibility identifier of the NoteEdit VC navigation bar done button.
extern NSString* const kNoteEditNavigationBarDoneButtonIdentifier;
// Accessibility identifier of the NoteFolderEdit VC navigation bar done
// button.
extern NSString* const kNoteFolderEditNavigationBarDoneButtonIdentifier;

// UIToolbar accessibility constants:
// Accessibility identifier of the NoteEditVC toolbar delete button.
extern NSString* const kNoteEditDeleteButtonIdentifier;
// Accessibility identifier of the NoteEditVC toolbar move button.
extern NSString* const kNoteEditMoveButtonIdentifier;
// Accessibility identifier of the NoteFolderEditorVC toolbar delete button.
extern NSString* const kNoteFolderEditorDeleteButtonIdentifier;
// Accessibility identifier of the NoteHomeVC leading button.
extern NSString* const kNoteHomeLeadingButtonIdentifier;
// Accessibility identifier of the NoteHomeVC center button.
extern NSString* const kNoteHomeCenterButtonIdentifier;
// Accessibility identifier of the NoteHomeVC trailing button.
extern NSString* const kNoteHomeTrailingButtonIdentifier;
// Accessibility identifier of the NoteHomeVC UIToolbar.
extern NSString* const kNoteHomeUIToolbarIdentifier;
// TODO - @karianne@vivaldi.com Add Search
// Accessibility identifier of the NoteHomeVC search bar.
extern NSString* const kNoteHomeSearchBarIdentifier;
// Accessibility identifier of the search scrim.
extern NSString* const kNoteHomeSearchScrimIdentifier;

// Cell Layout constants:
// The space between UIViews inside the cell.
extern const CGFloat kNoteCellViewSpacing;
// The vertical space between the Cell margin and its contents.
extern const CGFloat kNoteCellVerticalInset;
// The horizontal leading space between the Cell margin and its contents.
extern const CGFloat kNoteCellHorizontalLeadingInset;
// The horizontal space between the Cell content and its accessory view.
extern const CGFloat kNoteCellHorizontalAccessoryViewSpacing;

// Cell accessibility constants:
// Accessibility identifier of the Create NewFolder Button.
extern NSString* const kNoteCreateNewFolderCellIdentifier;

// Empty state accessibility constants:
// Accessibility identifier for the explanatory label in the empty state.
extern NSString* const kNoteEmptyStateExplanatoryLabelIdentifier;

extern NSString* const kToolsMenuNotesId;
extern NSString* const kToolsMenuAddToNotes;

#pragma mark - ICONS
// Image name for the note icon.
extern NSString* vNotesIcon;
// Image name for the note folder icon.
extern NSString* vNotesFolderIcon;
// Image name for trash folder icon
extern NSString* vNotesTrashFolderIcon;
// Image name for the folder selection chevron.
extern NSString* vNoteFolderSelectionChevron;
// Image name for the folder selection checkmark.
extern NSString* vNoteFolderSelectionCheckmark;
// Image name for add folder
extern NSString* vNoteAddFolder;

#pragma mark - SIZE
// Corner radius for the note body container view
extern CGFloat vNoteBodyCornerRadius;
// Note editor text view height
extern CGFloat vNoteTextViewHeight;
// Note folder selection header view height
extern CGFloat vNoteFolderSelectionHeaderViewHeight;

#pragma mark - OTHERS
// Maximum number of entries to fetch when searching on notes folder page.
extern const int vMaxNoteFolderSearchResults; // TODO

#pragma mark - MARKDOWN
extern NSString* vMarkdownLibraryBundleName;
extern NSString* vMarkdownHTMLFilename;

#endif  // IOS_UI_NOTES_NOTE_UI_CONSTANTS_H_
