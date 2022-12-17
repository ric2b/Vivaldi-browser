// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#import "ios/notes/note_ui_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSString* const kNoteHomeViewContainerIdentifier =
    @"kNoteHomeViewContainerIdentifier";
NSString* const kNoteEditViewContainerIdentifier =
    @"kNoteEditViewContainerIdentifier";
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
const CGFloat kNoteCellHorizontalTrailingInset = 24.0f;
const CGFloat kNoteCellHorizontalAccessoryViewSpacing = 11.0f;

NSString* const kNoteCreateNewFolderCellIdentifier =
    @"kNoteCreateNewFolderCellIdentifier";

NSString* const kNoteEmptyStateExplanatoryLabelIdentifier =
    @"kNoteEmptyStateExplanatoryLabelIdentifier";

#if BUILDFLAG(IS_IOS)
NSString* const kToolsMenuNotesId = @"kToolsMenuNotesId";
NSString* const kToolsMenuAddToNotes = @"kToolsMenuAddToNotes";
#endif
