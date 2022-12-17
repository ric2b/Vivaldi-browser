// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#ifndef IOS_NOTES_NOTE_FOLDER_EDITOR_VIEW_CONTROLLER_H_
#define IOS_NOTES_NOTE_FOLDER_EDITOR_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/table_view/chrome_table_view_controller.h"

@class NoteFolderEditorViewController;
class Browser;
@protocol SnackbarCommands;

namespace vivaldi {
class NotesModel;
class NoteNode;
}  // namespace notes

@protocol NoteFolderEditorViewControllerDelegate
// Called when the controller successfully created or edited |folder|.
- (void)noteFolderEditor:(NoteFolderEditorViewController*)folderEditor
      didFinishEditingFolder:(const vivaldi::NoteNode*)folder;
// Called when the user deletes the edited folder.
// This is never called if the editor is created with
// |folderCreatorWithNotesModel:parentFolder:|.
- (void)noteFolderEditorDidDeleteEditedFolder:
    (NoteFolderEditorViewController*)folderEditor;
// Called when the user cancels the folder creation.
- (void)noteFolderEditorDidCancel:
    (NoteFolderEditorViewController*)folderEditor;
// Called when the controller is going to commit the title change.
- (void)noteFolderEditorWillCommitTitleChange:
    (NoteFolderEditorViewController*)folderEditor;

@end

// View controller for creating or editing a note folder. Allows editing of
// the title and selecting the parent folder of the note.
// This controller monitors the state of the note model, so changes to the
// note model can affect this controller's state.
@interface NoteFolderEditorViewController
    : ChromeTableViewController <UIAdaptivePresentationControllerDelegate>

@property(nonatomic, weak) id<NoteFolderEditorViewControllerDelegate>
    delegate;

// Snackbar commands handler for this ViewController.
@property(nonatomic, weak) id<SnackbarCommands> snackbarCommandsHandler;

// Designated factory methods.

// Returns a view controller set to create a new folder in |parentFolder|.
// If |parentFolder| is NULL, a default parent will be set.
// |noteModel| must not be NULL and must be loaded.
+ (instancetype)
    folderCreatorWithNotesModel:(vivaldi::NotesModel*)noteModel
                      parentFolder:(const vivaldi::NoteNode*)parentFolder
                           browser:(Browser*)browser;

// |noteModel| must not be null and must be loaded.
// |folder| must not be NULL and be editable.
// |browser| must not be null.
+ (instancetype)
    folderEditorWithNotesModel:(vivaldi::NotesModel*)noteModel
                           folder:(const vivaldi::NoteNode*)folder
                          browser:(Browser*)browser;

- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

@end

#endif  // IOS_NOTES_NOTE_FOLDER_EDITOR_VIEW_CONTROLLER_H_
