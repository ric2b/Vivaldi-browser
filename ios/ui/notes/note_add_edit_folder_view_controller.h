// Copyright 2023 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_NOTES_NOTE_ADD_EDIT_FOLDER_VIEW_CONTROLLER_H_
#define IOS_UI_NOTES_NOTE_ADD_EDIT_FOLDER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "components/notes/note_node.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/ui/notes/note_add_edit_controller_delegate.h"
#import "ios/ui/notes/note_folder_chooser_view_controller.h"

using vivaldi::NoteNode;

// The controller responsible for adding and editing note FOLDER.
@interface NoteAddEditFolderViewController
    : UIViewController

// INITIALIZER
+ (instancetype)initWithBrowser:(Browser*)browser
                           item:(const NoteNode*)item
                         parent:(const NoteNode*)parent
                      isEditing:(BOOL)isEditing
                   allowsCancel:(BOOL)allowsCancel;

// DELEGATE
@property (nonatomic, weak)
  id<NoteAddEditControllerDelegate> delegate;

@end

#endif  // IOS_UI_NOTES_NOTE_ADD_EDIT_FOLDER_VIEW_CONTROLLER_H_
