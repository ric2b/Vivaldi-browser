// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#ifndef IOS_NOTES_NOTE_TRANSITIONING_DELEGATE_H_
#define IOS_NOTES_NOTE_TRANSITIONING_DELEGATE_H_

#import <UIKit/UIKit.h>

@protocol TableViewPresentationControllerDelegate;

@interface NoteTransitioningDelegate
    : NSObject<UIViewControllerTransitioningDelegate>

// The modal delegate that is passed along to the presentation controller.
@property(nonatomic, weak) id<TableViewPresentationControllerDelegate>
    presentationControllerModalDelegate;

@end

#endif  // IOS_NOTES_NOTE_TRANSITIONING_DELEGATE_H_
