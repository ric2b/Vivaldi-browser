// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#ifndef IOS_NOTES_NOTE_EDIT_VIEW_CONTROLLER_H_
#define IOS_NOTES_NOTE_EDIT_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/table_view/chrome_table_view_controller.h"

@class NoteEditViewController;
@class NoteFolderViewController;
@protocol BrowserCommands;
class Browser;
@protocol SnackbarCommands;

namespace vivaldi {
class NoteNode;
}  // namespace notes

@protocol NoteEditViewControllerDelegate

// Called when the edited note is set for deletion.
// If the delegate returns |YES|, all nodes matching the contet of |note| will
// be deleted.
// If the delegate returns |NO|, only |note| will be deleted.
// If the delegate doesn't implement this method, the default behavior is to
// delete all nodes matching the content of |note|.
- (BOOL)noteEditor:(NoteEditViewController*)controller
    shoudDeleteAllOccurencesOfNote:(const vivaldi::NoteNode*)note;

// Called when the controller should be dismissed.
- (void)noteEditorWantsDismissal:(NoteEditViewController*)controller;

// Called when the controller is going to commit the title or content change.
- (void)noteEditorWillCommitContentChange:
    (NoteEditViewController*)controller;

@end

// View controller for editing notes. Allows editing of the content.
// Currently not the parent folder of the note.
//
// This view controller will also monitor note model change events and react
// accordingly depending on whether the note and folder it is editing
// changes underneath it.
@interface NoteEditViewController
    : ChromeTableViewController <UIAdaptivePresentationControllerDelegate>

@property(nonatomic, weak) id<NoteEditViewControllerDelegate> delegate;

// Snackbar commands handler.
@property(nonatomic, weak) id<SnackbarCommands> snackbarCommandsHandler;

// Designated initializer.
// |note|: mustn't be NULL at initialization time. It also mustn't be a
//             folder.
- (instancetype)initWithNote:(const vivaldi::NoteNode*)note
                         browser:(Browser*)browser NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

// Called before the instance is deallocated.
- (void)shutdown;

// Closes the edit view as if close button was pressed.
- (void)dismiss;

@end

#endif  // IOS_NOTES_NOTE_EDIT_VIEW_CONTROLLER_H_
