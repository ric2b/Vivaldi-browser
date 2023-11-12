// Copyright 2023 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_MODAL_PAGE_MODAL_PAGE_COMMANDS_H_
#define IOS_UI_MODAL_PAGE_MODAL_PAGE_COMMANDS_H_

// Commands for displaying a webpage in modal view with a top
// navigation bar with a button which should dismiss the page. Those commands
// should manage the modal page coordinator.
@protocol ModalPageCommands

- (void)showModalPage:(NSURL*)url title:(NSString*)title;
- (void)closeModalPage;

@end

#endif  // IOS_UI_MODAL_PAGE_MODAL_PAGE_COMMANDS_H_
