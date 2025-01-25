// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#ifndef IOS_UI_NOTES_NOTE_ADD_EDIT_VIEW_CONTROLLER_H_
#define IOS_UI_NOTES_NOTE_ADD_EDIT_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "components/notes/notes_model.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller.h"

@class NoteAddEditViewController;
@class NoteFolderChooserViewController;
@protocol BrowserCommands;
class Browser;
@protocol SnackbarCommands;

namespace vivaldi {
class NoteNode;
}  // namespace vivaldi

@protocol NoteAddEditViewControllerDelegate

// Called when the edited note is set for deletion.
// If the delegate returns |YES|, all nodes matching the content of |note| will
// be deleted.
// If the delegate returns |NO|, only |note| will be deleted.
// If the delegate doesn't implement this method, the default behavior is to
// delete all nodes matching the content of |note|.
- (BOOL)noteEditor:(NoteAddEditViewController*)controller
    shoudDeleteAllOccurencesOfNote:(const vivaldi::NoteNode*)note;

// Called when the controller should be dismissed.
- (void)noteEditorWantsDismissal:(NoteAddEditViewController*)controller;

// Called when the controller is going to commit the title or content change.
- (void)noteEditorWillCommitContentChange:
    (NoteAddEditViewController*)controller;

@end

// View controller for editing notes. Allows editing of the content.
// Currently not the parent folder of the note.
//
// This view controller will also monitor note model change events and react
// accordingly depending on whether the note and folder it is editing
// changes underneath it.
@interface NoteAddEditViewController
    : UIViewController

@property(nonatomic, weak) id<NoteAddEditViewControllerDelegate> delegate;
@property(nonatomic, strong) UIBarButtonItem *toggleButton;
@property(nonatomic, assign) BOOL isToggledOn;

// Snackbar commands handler.
@property(nonatomic, weak) id<SnackbarCommands> snackbarCommandsHandler;

// Initializer.
// |note|: mustn't be NULL at initialization time. It also mustn't be a
//             folder.
+ (instancetype)initWithBrowser:(Browser*)browser
                           item:(const vivaldi::NoteNode*)note
                         parent:(const vivaldi::NoteNode*)parent
                      isEditing:(BOOL)isEditing
                   allowsCancel:(BOOL)allowsCancel;

- (instancetype)initWithBrowser:(Browser*)browser;

// Closes the edit view as if close button was pressed.
- (void)dismiss;

@end

#endif  // IOS_UI_NOTES_NOTE_ADD_EDIT_VIEW_CONTROLLER_H_
