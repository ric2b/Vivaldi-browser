// Copyright (c) 2023 Vivaldi Technologies AS. All rights reserved

#ifndef IOS_UI_NOTES_NOTE_FOLDER_CHOOSER_VIEW_CONTROLLER_H_
#define IOS_UI_NOTES_NOTE_FOLDER_CHOOSER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import <set>

#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller.h"

@class NoteFolderChooserViewController;
class Browser;
@protocol SnackbarCommands;

namespace vivaldi {
class NotesModel;
class NoteNode;
}  // namespace notes

@protocol NoteFolderChooserViewControllerDelegate
// Called when a note folder is selected. |folder| is the newly selected
// folder.
- (void)folderPicker:(NoteFolderChooserViewController*)folderPicker
    didFinishWithFolder:(const vivaldi::NoteNode*)folder;
// Called when the user is done with the picker, either by tapping the Cancel or
// the Back button.
- (void)folderPickerDidCancel:(NoteFolderChooserViewController*)folderPicker;
- (void)folderPickerDidDismiss:(NoteFolderChooserViewController*)folderPicker;
@end

// A folder selector view controller.
//
// This controller monitors the state of the note model, so changes to the
// note model can affect this controller's state.
// The note model is assumed to be loaded, thus also not to be NULL.
@interface NoteFolderChooserViewController
    : LegacyChromeTableViewController <UIAdaptivePresentationControllerDelegate>

@property(nonatomic, weak) id<NoteFolderChooserViewControllerDelegate> delegate;

// Handler for Snackbar Commands.
@property(nonatomic, weak) id<SnackbarCommands> snackbarCommandsHandler;

// The current nodes (notes or folders) that are considered for a move.
@property(nonatomic, assign, readonly)
    std::set<const vivaldi::NoteNode*>& editedNodes;

// Initializes the view controller with a notes model. |allowsNewFolders|
// will instruct the controller to provide the necessary UI to create a folder.
// |noteModel| must not be NULL and must be loaded.
// |editedNodes| affects which cells can be selected, since it is not possible
// to move a node into its subnode.
// |allowsCancel| puts a cancel and done button in the navigation bar instead of
// a back button, which is needed if this view controller is presented modally.
- (instancetype)
    initWithNotesModel:(vivaldi::NotesModel*)noteModel
         allowsNewFolders:(BOOL)allowsNewFolders
              editedNodes:(const std::set<const vivaldi::NoteNode*>&)nodes
             allowsCancel:(BOOL)allowsCancel
           selectedFolder:(const vivaldi::NoteNode*)selectedFolder
                  browser:(Browser*)browser;

// This method changes the currently selected folder and updates the UI. The
// delegate is not notified of the change.
- (void)changeSelectedFolder:(const vivaldi::NoteNode*)selectedFolder;

@end

#endif  // IOS_UI_NOTES_NOTE_FOLDER_CHOOSER_VIEW_CONTROLLER_H_
