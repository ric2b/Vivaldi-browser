// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#ifndef IOS_NOTES_NOTE_INTERACTION_CONTROLLER_H_
#define IOS_NOTES_NOTE_INTERACTION_CONTROLLER_H_

#import <UIKit/UIKit.h>

@protocol ApplicationCommands;
class Browser;
@protocol BrowserCommands;
@protocol NoteInteractionControllerDelegate;

namespace vivaldi {
class NoteNode;
}

class GURL;

namespace web {
class WebState;
}

// The NoteInteractionController abstracts the management of the various
// UIViewControllers used to create, remove and edit a note.
@interface NoteInteractionController : NSObject

// This object's delegate.
@property(nonatomic, weak) id<NoteInteractionControllerDelegate> delegate;

- (instancetype)initWithBrowser:(Browser*)browser
               parentController:(UIViewController*)parentController
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Called before the instance is deallocated.
- (void)shutdown;

// Presents the notes browser modally.
- (void)presentNotes;

// Presents the note or folder editor for the given |node|.
- (void)presentEditorForNode:(const vivaldi::NoteNode*)node;

// Removes any note modal controller from view if visible.
// override this method.
- (void)dismissNoteModalControllerAnimated:(BOOL)animated;

// Removes any snackbar related to notes that could have been presented.
- (void)dismissSnackbar;

@end

#endif  // IOS_NOTES_NOTE_INTERACTION_CONTROLLER_H_
