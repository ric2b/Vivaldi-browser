// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#ifndef IOS_UI_NOTES_NOTE_NAVIGATION_CONTROLLER_DELEGATE_H_
#define IOS_UI_NOTES_NOTE_NAVIGATION_CONTROLLER_DELEGATE_H_

#import <UIKit/UIKit.h>

@protocol TableViewModalPresenting;

// NoteNavigationControllerDelegate serves as a delegate for
// TableViewNavigationController. It uses |modalController| to update the modal
// presentation state when view controllers are pushed onto or popped off of the
// navigation stack.
@interface NoteNavigationControllerDelegate
    : NSObject <UINavigationControllerDelegate>

@end

#endif  // IOS_UI_NOTES_NOTE_NAVIGATION_CONTROLLER_DELEGATE_H_
