// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#import "ios/ui/bookmarks_editor/vivaldi_bookmark_prefs.h"

#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/pref_service.h"
#import "prefs/vivaldi_pref_names.h"


@implementation VivaldiBookmarkPrefs

static PrefService *_prefService = nil;

+ (PrefService*)prefService {
    return _prefService;
}

+ (void)setPrefService:(PrefService*)pref {
    _prefService = pref;
}

+ (void)registerBrowserStatePrefs:(user_prefs::PrefRegistrySyncable*)registry {
  registry->RegisterIntegerPref(vivaldiprefs::kVivaldiBookmarksSortingMode,
                                VivaldiBookmarksSortingModeManual);
  registry->RegisterIntegerPref(vivaldiprefs::kVivaldiBookmarksSortingOrder,
                                VivaldiBookmarksSortingOrderAscending);
  registry->RegisterBooleanPref(vivaldiprefs::kVivaldiBookmarkFoldersViewMode,
                                false);
}

#pragma mark - Getters
+ (const VivaldiBookmarksSortingMode)getBookmarksSortingMode {
  PrefService *prefService = [VivaldiBookmarkPrefs prefService];
  int modeIndex =
    prefService->GetInteger(vivaldiprefs::kVivaldiBookmarksSortingMode);

  switch (modeIndex) {
    case VivaldiBookmarksSortingModeManual:
      return VivaldiBookmarksSortingModeManual;
    case 1:
      return VivaldiBookmarksSortingModeByTitle;
    case 2:
      return VivaldiBookmarksSortingModeByAddress;
    case 3:
      return VivaldiBookmarksSortingModeByNickname;
    case 4:
      return VivaldiBookmarksSortingModeByDescription;
    case 5:
      return VivaldiBookmarksSortingModeByDate;
    case 6:
      return VivaldiBookmarksSortingModeByKind;
    default:
      return VivaldiBookmarksSortingModeManual;
  }
}

+ (const VivaldiBookmarksSortingOrder)getBookmarksSortingOrder {
  PrefService *prefService = [VivaldiBookmarkPrefs prefService];
  int orderIndex =
      prefService->GetInteger(vivaldiprefs::kVivaldiBookmarksSortingOrder);
  switch (orderIndex) {
    case 0:
      return VivaldiBookmarksSortingOrderAscending;
    case 1:
      return VivaldiBookmarksSortingOrderDescending;
    default:
      return VivaldiBookmarksSortingOrderAscending;
  }
}

+ (BOOL)getFolderViewMode {
  PrefService *prefService = [VivaldiBookmarkPrefs prefService];
  return prefService->GetBoolean(vivaldiprefs::kVivaldiBookmarkFoldersViewMode);
}

#pragma mark - Setters

+ (void)setBookmarksSortingMode:(const VivaldiBookmarksSortingMode)mode {
  PrefService *prefService = [VivaldiBookmarkPrefs prefService];
  prefService->SetInteger(vivaldiprefs::kVivaldiBookmarksSortingMode, mode);
}

+ (void)setBookmarksSortingOrder:(const VivaldiBookmarksSortingOrder)order {
  PrefService *prefService = [VivaldiBookmarkPrefs prefService];
  prefService->SetInteger(vivaldiprefs::kVivaldiBookmarksSortingOrder, order);
}

+ (void)setFolderViewMode:(BOOL)showOnlySpeedDials {
  PrefService *prefService = [VivaldiBookmarkPrefs prefService];
  prefService->SetBoolean(vivaldiprefs::kVivaldiBookmarkFoldersViewMode,
                          showOnlySpeedDials);
}

@end
