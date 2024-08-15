// Copyright 2023 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_NOTES_NOTE_PARENT_FOLDER_VIEW_H_
#define IOS_UI_NOTES_NOTE_PARENT_FOLDER_VIEW_H_

#import <UIKit/UIKit.h>

#import "components/notes/note_node.h"

@protocol NoteParentFolderViewDelegate
- (void) didTapParentFolder;
@end

// A view to hold folder selection view for the note editor
@interface NoteParentFolderView : UIView

- (instancetype)initWithTitle:(NSString*)title NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

// DELEGATE
@property (nonatomic, weak) id<NoteParentFolderViewDelegate> delegate;

// SETTERS
- (void)setParentFolderAttributesWithItem:(const vivaldi::NoteNode*)item;

@end

#endif  // IOS_UI_NOTES_NOTE_PARENT_FOLDER_VIEW_H_
