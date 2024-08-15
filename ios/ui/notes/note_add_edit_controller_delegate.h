// Copyright 2023 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_NOTES_NOTE_ADD_EDIT_CONTROLLER_DELEGATE_H_
#define IOS_UI_NOTES_NOTE_ADD_EDIT_CONTROLLER_DELEGATE_H_

#import "components/notes/notes_model.h"

@protocol NoteAddEditControllerDelegate
- (void)didCreateNewFolder:(const vivaldi::NoteNode*)folder;
@end

#endif  // IOS_UI_NOTES_NOTE_ADD_EDIT_CONTROLLER_DELEGATE_H_
