// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#import "ios/ui/bookmarks_editor/vivaldi_bookmark_prefs.h"

#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/pref_service.h"
#import "prefs/vivaldi_pref_names.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation VivaldiBookmarkPrefs

+ (void)registerBrowserStatePrefs:(user_prefs::PrefRegistrySyncable*)registry {
  registry->RegisterBooleanPref(vivaldiprefs::kVivaldiBookmarkFoldersViewMode,
                                false);
}

+ (BOOL)getFolderViewModeFromPrefService:(PrefService*)prefService {
  return prefService->GetBoolean(vivaldiprefs::kVivaldiBookmarkFoldersViewMode);
}

+ (void)setFolderViewMode:(BOOL)showOnlySpeedDials
           inPrefServices:(PrefService*)prefService {
  prefService->SetBoolean(vivaldiprefs::kVivaldiBookmarkFoldersViewMode,
                          showOnlySpeedDials);
}

@end
