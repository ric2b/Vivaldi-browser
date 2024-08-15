// Copyright 2024 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_COPY_TO_NOTE_COPY_TO_NOTE_DELEGATE_H_
#define IOS_UI_COPY_TO_NOTE_COPY_TO_NOTE_DELEGATE_H_

@protocol UIMenuBuilder;

// Protocol for handling copy to note and presenting related UI.
@protocol CopyToNoteDelegate

// Handles the copy to note menu item selection.
- (void)handleCopyToNoteSelection;

// Will be called by `BrowserContainerViewController buildMenuWithBuilder:`
// to customize its edit menu.
- (void)buildMenuWithBuilder:(id<UIMenuBuilder>)builder;

@end

#endif  // IOS_UI_COPY_TO_NOTE_COPY_TO_NOTE_DELEGATE_H_
