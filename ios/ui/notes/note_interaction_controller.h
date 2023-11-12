// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#ifndef IOS_UI_NOTES_NOTE_INTERACTION_CONTROLLER_H_
#define IOS_UI_NOTES_NOTE_INTERACTION_CONTROLLER_H_

#import <UIKit/UIKit.h>

@protocol ApplicationCommands;
class Browser;
@protocol BrowserCommands;
@protocol NoteInteractionControllerDelegate;
@protocol SnackbarCommands;

namespace vivaldi {
class NoteNode;
}

class GURL;

namespace web {
class WebState;
}

@class PanelInteractionController;

// The NoteInteractionController abstracts the management of the various
// UIViewControllers used to create, remove and edit a note.
@interface NoteInteractionController : NSObject

// This object's delegate.
@property(nonatomic, weak) id<NoteInteractionControllerDelegate> delegate;

// The base view controller for this coordinator
@property(nonatomic, weak, readwrite) UIViewController* baseViewController;

@property(nonatomic, weak) PanelInteractionController* panelDelegate;

// The navigation controller that is being presented, if any.
// |self.noteBrowser|, |self.noteEditor|, and |self.folderEditor| are
// children of this navigation controller.
@property(nonatomic, strong) UINavigationController* noteNavigationController;

- (instancetype)initWithBrowser:(Browser*)browser
               parentController:(UIViewController*)parentController
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Called before the instance is deallocated.
- (void)shutdown;

// Opens the notes browser in the modal panel.
- (void)showNotes;

// Presents the note or folder editor for the given |node|.
- (void)presentEditorForNode:(const vivaldi::NoteNode*)node;

// Presents the note add view controller
- (void)presentAddViewController:(const vivaldi::NoteNode*)parent;

// Removes any note modal controller from view if visible.
// override this method.
- (void)dismissNoteModalControllerAnimated:(BOOL)animated;

// Removes any snackbar related to notes that could have been presented.
- (void)dismissSnackbar;

- (void)presentNoteFolderEditor:(const vivaldi::NoteNode*)node
                         parent:(const vivaldi::NoteNode*)parentNode
                      isEditing:(BOOL)isEditing;

@end

#endif  // IOS_UI_NOTES_NOTE_INTERACTION_CONTROLLER_H_
