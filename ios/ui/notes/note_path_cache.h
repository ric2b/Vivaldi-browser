// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#ifndef IOS_UI_NOTES_NOTE_PATH_CACHE_H_
#define IOS_UI_NOTES_NOTE_PATH_CACHE_H_

#import <UIKit/UIKit.h>

namespace vivaldi {
class NotesModel;
}  // namespace vivaldi

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

class PrefService;

// Stores and retrieves the note top most row that the user was last
// viewing.
@interface NotePathCache : NSObject

// Registers the feature preferences.
+ (void)registerBrowserStatePrefs:(user_prefs::PrefRegistrySyncable*)registry;

// Caches the note top most row that the user was last viewing.
+ (void)cacheNoteTopMostRowWithPrefService:(PrefService*)prefService
                                      folderId:(int64_t)folderId
                                    topMostRow:(int)topMostRow;

// Gets the note top most row that the user was last viewing. Returns YES if
// a valid cache exists. |folderId| and |topMostRow| are out variables, only
// populated if the return is YES.
+ (BOOL)getNoteTopMostRowCacheWithPrefService:(PrefService*)prefService
                                            model:
                                                (vivaldi::NotesModel*)model
                                         folderId:(int64_t*)folderId
                                       topMostRow:(int*)topMostRow;

// Clears the note top most row cache.
+ (void)clearNoteTopMostRowCacheWithPrefService:(PrefService*)prefService;

@end

#endif  // IOS_UI_NOTES_NOTE_PATH_CACHE_H_
