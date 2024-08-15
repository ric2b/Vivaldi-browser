// Copyright 2024 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_COPY_TO_NOTE_COPY_TO_NOTE_MEDIATOR_H_
#define IOS_UI_COPY_TO_NOTE_COPY_TO_NOTE_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/ui/copy_to_note/copy_to_note_delegate.h"

@protocol ActivityServiceCommands;
@protocol EditMenuAlertDelegate;
class Browser;

// Mediator that mediates between the browser container views and the
// copy_to_note tab helpers.
@interface CopyToNoteMediator : NSObject <CopyToNoteDelegate>

// Initializer for a mediator. `Browser` is the
// Browser whose content is shown within the BrowserContainerConsumer. It must
// be non-null. `consumer` is the consumer of copy-to-note updates.
- (instancetype)initWithBrowser:(Browser*)browser
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// The delegate to present error message alerts.
@property(nonatomic, weak) id<EditMenuAlertDelegate> alertDelegate;

// The handler for activity services commands.
@property(nonatomic, weak) id<ActivityServiceCommands> activityServiceHandler;

// Handles the copy to note menu item selection.
- (void)handleCopyToNoteSelection;

@end

#endif  // IOS_CHROME_BROWSER_UI_LINK_TO_TEXT_LINK_TO_TEXT_MEDIATOR_H_
