// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#import "ios/ui/notes/vivaldi_notes_pref.h"

#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/pref_service.h"
#import "ios/ui/helpers/vivaldi_global_helpers.h"
#import "prefs/vivaldi_pref_names.h"

namespace {
  // setting 3 for default mode to NotesDateEditedSort
  const int kDefaultSortingMode = 3;
}

@implementation VivaldiNotesPrefs

static PrefService *_prefService = nil;

+ (PrefService*)prefService {
    return _prefService;
}

+ (void)setPrefService:(PrefService*)pref {
    _prefService = pref;
}

+ (void)registerBrowserStatePrefs:(user_prefs::PrefRegistrySyncable*)registry {
   registry->RegisterIntegerPref(vivaldiprefs::kVivaldiNotesSortingMode, kDefaultSortingMode);
   registry->RegisterBooleanPref(vivaldiprefs::kVivaldiNotesSortingOrder, NotesSortingOrderAscending);
}

#pragma mark - GETTERS
/// Returns the  notes sorting mode from prefs.
+ (const NotesSortingMode)getNotesSortingMode {
  PrefService *prefService = [VivaldiNotesPrefs prefService];
  int modeIndex =
    prefService->GetInteger(vivaldiprefs::kVivaldiNotesSortingMode);

  switch (modeIndex) {
    case 0:
      return NotesSortingModeManual;
    case 1:
      return NotesSortingModeTitle;
    case 2:
      return NotesSortingModeDateCreated;
    case 3:
      return NotesSortingModeDateEdited;
    case 4:
      return NotesSortingModeByKind;
    default:
      return NotesSortingModeDateEdited;
  }
}

/// Returns the  notes sorting order from prefs.
+ (const NotesSortingOrder)getNotesSortingOrder {
  PrefService *prefService = [VivaldiNotesPrefs prefService];
  return prefService->GetBoolean(vivaldiprefs::kVivaldiNotesSortingOrder)
    ? NotesSortingOrderDescending
      : NotesSortingOrderAscending;
}

#pragma mark - SETTERS
/// Sets the  notes sorting mode to the prefs.
+ (void)setNotesSortingMode:(const NotesSortingMode)mode {
  PrefService *prefService = [VivaldiNotesPrefs prefService];
  prefService->SetInteger(vivaldiprefs::kVivaldiNotesSortingMode, mode);
}

/// Sets the  notes sorting order to the prefs.
+ (void)setNotesSortingOrder:(const NotesSortingOrder)order {
  PrefService *prefService = [VivaldiNotesPrefs prefService];
  prefService->SetBoolean(vivaldiprefs::kVivaldiNotesSortingOrder, order);
}

@end
