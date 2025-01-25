// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#ifndef IOS_UI_NOTES_NOTE_HOME_MEDIATOR_H_
#define IOS_UI_NOTES_NOTE_HOME_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "base/memory/raw_ptr.h"
#import "base/strings/utf_offset_string_conversions.h"

class ProfileIOS;
@protocol NoteHomeConsumer;
@class NoteHomeSharedState;

// NoteHomeMediator manages model interactions for the
// NoteHomeViewController.
@interface NoteHomeMediator : NSObject

@property(nonatomic, weak) id<NoteHomeConsumer> consumer;

- (instancetype)initWithSharedState:(NoteHomeSharedState*)sharedState
                            profile:(ProfileIOS*)profile
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Starts this mediator. Populates the table view model with current data and
// begins listening for backend model updates.
- (void)startMediating;

// Stops mediating and disconnects from backend models.
- (void)disconnect;

// Rebuilds the table view model data for the Notes section.  Deletes any
// existing data first.
- (void)computeNoteTableViewData;

// Rebuilds the table view model data for the notes matching the given text.
// Deletes any existing data first.  If no items found, an entry with
// |noResults' message is added to the table.
- (void)computeNoteTableViewDataMatching:(NSString*)searchText
                  orShowMessageWhenNoResults:(NSString*)noResults;
@end

#endif  // IOS_UI_NOTES_NOTE_HOME_MEDIATOR_H_
