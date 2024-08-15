// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#ifndef IOS_UI_NOTES_VIVALDI_NOTES_PREFS_H_
#define IOS_UI_NOTES_VIVALDI_NOTES_PREFS_H_

#import <UIKit/UIKit.h>

#import "ios/ui/notes/note_sorting_mode.h"

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

class PrefService;

// Stores and retrieves the prefs for the notes
@interface VivaldiNotesPrefs : NSObject

// Static variable declaration
+ (PrefService*)prefService;

// Static method to set the PrefService
+ (void)setPrefService:(PrefService *)pref;

/// Registers the feature preferences.
+ (void)registerBrowserStatePrefs:(user_prefs::PrefRegistrySyncable*)registry;

#pragma mark - Getters
/// Returns the  notes sorting mode from prefs.
+ (const NotesSortingMode)getNotesSortingMode;

/// Returns the  notes sorting order from prefs.
+ (const NotesSortingOrder)getNotesSortingOrder;

#pragma mark - Setters
/// Sets the  notes sorting mode to the prefs.
+ (void)setNotesSortingMode:(const NotesSortingMode)mode;

/// Sets the  notes sorting order to the prefs.
+ (void)setNotesSortingOrder:(const NotesSortingOrder)order;
@end

#endif  // IOS_UI_NOTES_VIVALDI_NOTES_PREFS_H_
