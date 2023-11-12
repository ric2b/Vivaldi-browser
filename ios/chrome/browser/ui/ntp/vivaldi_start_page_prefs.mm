// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#import "ios/chrome/browser/ui/ntp/vivaldi_start_page_prefs.h"

#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "prefs/vivaldi_pref_names.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation VivaldiStartPagePrefs

+ (void)registerBrowserStatePrefs:(user_prefs::PrefRegistrySyncable*)registry {
  registry->RegisterIntegerPref(vivaldiprefs::kVivaldiSpeedDialSortingMode,
                                SpeedDialSortingManual);
  registry->RegisterIntegerPref(vivaldiprefs::kVivaldiStartPageLayoutStyle,
                                VivaldiStartPageLayoutStyleMedium);
}

#pragma mark - GETTERS

+ (const SpeedDialSortingMode)getSDSortingModeWithPrefService:
    (PrefService*)prefService {
  int modeIndex =
    prefService->GetInteger(vivaldiprefs::kVivaldiSpeedDialSortingMode);

  switch (modeIndex) {
    case SpeedDialSortingManual:
      return SpeedDialSortingManual;
    case 1:
      return SpeedDialSortingByTitle;
    case 2:
      return SpeedDialSortingByAddress;
    case 3:
      return SpeedDialSortingByNickname;
    case 4:
      return SpeedDialSortingByDescription;
    case 5:
      return SpeedDialSortingByDate;
    default:
      return SpeedDialSortingManual;
  }
}

+ (const VivaldiStartPageLayoutStyle)getStartPageLayoutStyleWithPrefService:
    (PrefService*)prefService {
  int style =
    prefService->GetInteger(vivaldiprefs::kVivaldiStartPageLayoutStyle);

  switch (style) {
    case 0:
      return VivaldiStartPageLayoutStyleLarge;
    case 1:
      return VivaldiStartPageLayoutStyleMedium;
    case 2:
      return VivaldiStartPageLayoutStyleSmall;
    case 3:
      return VivaldiStartPageLayoutStyleList;
    default:
      return VivaldiStartPageLayoutStyleMedium;
  }
}

#pragma mark - SETTERS

+ (void)setSDSortingMode:(const SpeedDialSortingMode)mode
          inPrefServices:(PrefService*)prefService {
  prefService->SetInteger(vivaldiprefs::kVivaldiSpeedDialSortingMode, mode);
}

+ (void)setStartPageLayoutStyle:(const VivaldiStartPageLayoutStyle)style
                 inPrefServices:(PrefService*)prefService  {
  prefService->SetInteger(vivaldiprefs::kVivaldiStartPageLayoutStyle, style);
}

@end
