// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#ifndef IOS_UI_NOTES_NOTE_EMPTY_BACKGROUND_H_
#define IOS_UI_NOTES_NOTE_EMPTY_BACKGROUND_H_

#import <UIKit/UIKit.h>

// View shown as a background view of the note table view when there are no
// notes in the table view.
@interface NoteEmptyBackground : UIView

// The text displayed in the center of the view, below a large star icon.
@property(nonatomic, copy) NSString* text;

@end

#endif  // IOS_UI_NOTES_NOTE_EMPTY_BACKGROUND_H_
