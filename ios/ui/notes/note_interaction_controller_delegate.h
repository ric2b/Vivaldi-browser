// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#ifndef IOS_UI_NOTES_NOTE_INTERACTION_CONTROLLER_DELEGATE_H_
#define IOS_UI_NOTES_NOTE_INTERACTION_CONTROLLER_DELEGATE_H_

@class NoteInteractionController;

// NoteInteractionControllerDelegate provides methods for the controller to
// notify its delegate when certain events occur.
@protocol NoteInteractionControllerDelegate

// Called when the controller is going to commit the title or Content change.
- (void)noteInteractionControllerWillCommitContentChange:
    (NoteInteractionController*)controller;

// Called when the controller is stopped and the receiver can safely free any
// references to |controller|.
- (void)noteInteractionControllerDidStop:
    (NoteInteractionController*)controller;

@end

#endif  // IOS_UI_NOTES_NOTE_INTERACTION_CONTROLLER_DELEGATE_H_
