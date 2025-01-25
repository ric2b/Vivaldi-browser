// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#ifndef IOS_UI_NOTES_NOTE_MEDIATOR_H_
#define IOS_UI_NOTES_NOTE_MEDIATOR_H_

#import <UIKit/UIKit.h>

class ProfileIOS;

namespace vivaldi {
class NoteNode;
}  // namespace vivaldi

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

class GURL; // TODO
@class MDCSnackbarMessage;
@class URLWithTitle;

// Mediator for the notes.
@interface NoteMediator : NSObject

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithProfile:(ProfileIOS*)profile
    NS_DESIGNATED_INITIALIZER;

// Registers the feature preferences.
+ (void)registerBrowserStatePrefs:(user_prefs::PrefRegistrySyncable*)registry;

// Accesses the default folder for notes. The default folder is Mobile
// Notes.
+ (const vivaldi::NoteNode*)folderForNewNotesInProfile:(ProfileIOS*)profile;
+ (void)setFolderForNewNotes:(const vivaldi::NoteNode*)folder
                   inProfile:(ProfileIOS*)profile;

@end

#endif  // IOS_UI_NOTES_NOTE_MEDIATOR_H_
