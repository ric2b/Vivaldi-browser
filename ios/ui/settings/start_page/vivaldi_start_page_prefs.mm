// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#import "ios/ui/settings/start_page/vivaldi_start_page_prefs.h"

#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/pref_service.h"
#import "ios/ui/helpers/vivaldi_global_helpers.h"
#import "prefs/vivaldi_pref_names.h"

@implementation VivaldiStartPagePrefs

static PrefService *_prefService = nil;
static PrefService *_localPrefService = nil;

+ (PrefService*)prefService {
  return _prefService;
}

+ (PrefService*)localPrefService {
  return _localPrefService;
}

+ (void)setPrefService:(PrefService*)pref {
  _prefService = pref;
}

+ (void)setLocalPrefService:(PrefService*)pref {
  _localPrefService = pref;
}

+ (void)registerBrowserStatePrefs:(user_prefs::PrefRegistrySyncable*)registry {
  registry->RegisterIntegerPref(vivaldiprefs::kVivaldiSpeedDialSortingMode,
                                SpeedDialSortingManual);
  registry->RegisterIntegerPref(vivaldiprefs::kVivaldiSpeedDialSortingOrder,
                                SpeedDialSortingOrderAscending);
  registry->RegisterIntegerPref(vivaldiprefs::kVivaldiStartPageLayoutStyle,
                                VivaldiStartPageLayoutStyleSmall);
  registry->RegisterIntegerPref(vivaldiprefs::kVivaldiStartPageSDMaximumColumns,
                                VivaldiStartPageLayoutColumnUnlimited);
  registry->RegisterBooleanPref(
      vivaldiprefs::kVivaldiStartPageShowFrequentlyVisited, NO);
  registry->RegisterBooleanPref(
      vivaldiprefs::kVivaldiStartPageShowSpeedDials, YES);
  registry->RegisterBooleanPref(
      vivaldiprefs::kVivaldiStartPageShowCustomizeButton, NO);
  registry->RegisterStringPref(vivaldiprefs::kVivaldiStartupWallpaper, "");
  registry->RegisterStringPref(vivaldiprefs::kVivaldiStartpagePortraitImage,"");
  registry->RegisterStringPref(vivaldiprefs::kVivaldiStartpageLandscapeImage,"");
}

+ (void)registerLocalStatePrefs:(PrefRegistrySimple*)registry {
  registry->RegisterIntegerPref(vivaldiprefs::kVivaldiStartPageOpenWithItem,
                                VivaldiStartPageStartItemTypeFirstGroup);
  registry->RegisterIntegerPref(vivaldiprefs::kVivaldiStartPageLastVisitedGroup,
                                0);
}

#pragma mark - GETTERS

+ (const SpeedDialSortingMode)getSDSortingMode {
  PrefService *prefService = [VivaldiStartPagePrefs prefService];
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
    case 6:
      return SpeedDialSortingByKind;
    default:
      return SpeedDialSortingManual;
  }
}

+ (const SpeedDialSortingOrder)getSDSortingOrder {
  PrefService *prefService = [VivaldiStartPagePrefs prefService];
  int orderIndex =
      prefService->GetInteger(vivaldiprefs::kVivaldiSpeedDialSortingOrder);
  switch (orderIndex) {
    case 0:
      return SpeedDialSortingOrderAscending;
    case 1:
      return SpeedDialSortingOrderDescending;
    default:
      return SpeedDialSortingOrderAscending;
  }
}

+ (const VivaldiStartPageLayoutStyle)getStartPageLayoutStyle {
  PrefService *prefService = [VivaldiStartPagePrefs prefService];
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
      return VivaldiStartPageLayoutStyleSmall;
  }
}

+ (const VivaldiStartPageLayoutColumn)getStartPageSpeedDialMaximumColumns {
  PrefService *prefService = [VivaldiStartPagePrefs prefService];
  int style =
      prefService->GetInteger(vivaldiprefs::kVivaldiStartPageSDMaximumColumns);

  switch (style) {
    case 1:
      return VivaldiStartPageLayoutColumnOne;
    case 2:
      return VivaldiStartPageLayoutColumnTwo;
    case 3:
      return VivaldiStartPageLayoutColumnThree;
    case 4:
      return VivaldiStartPageLayoutColumnFour;
    case 5:
      return VivaldiStartPageLayoutColumnFive;
    case 6:
      return VivaldiStartPageLayoutColumnSix;
    case 7:
      return VivaldiStartPageLayoutColumnSeven;
    case 8:
      return VivaldiStartPageLayoutColumnEight;
    case 10:
      return VivaldiStartPageLayoutColumnTen;
    case 12:
      return VivaldiStartPageLayoutColumnTwelve;
    case 5000:
      return VivaldiStartPageLayoutColumnUnlimited;
    default:
      return VivaldiStartPageLayoutColumnUnlimited;
  }
}

+ (BOOL)showFrequentlyVisitedPages {
  PrefService *prefService = [VivaldiStartPagePrefs prefService];
  return prefService->GetBoolean(
      vivaldiprefs::kVivaldiStartPageShowFrequentlyVisited);
}

+ (BOOL)showSpeedDials {
  PrefService *prefService = [VivaldiStartPagePrefs prefService];
  return prefService->GetBoolean(
      vivaldiprefs::kVivaldiStartPageShowSpeedDials);
}

+ (const VivaldiStartPageStartItemType)getReopenStartPageWithItem {
  PrefService *prefService = [VivaldiStartPagePrefs localPrefService];
  int item =
      prefService->GetInteger(vivaldiprefs::kVivaldiStartPageOpenWithItem);

  // Cast the integer directly to the enum type,
  // assuming it's within the valid range.
  VivaldiStartPageStartItemType itemType = (VivaldiStartPageStartItemType)item;

  // Check if the cast value is within the valid enum range.
  if (itemType >= VivaldiStartPageStartItemTypeFirstGroup &&
      itemType <= VivaldiStartPageStartItemTypeLastVisited) {
    return itemType;
  }

  // Default fallback.
  return VivaldiStartPageStartItemTypeFirstGroup;
}

+ (const NSInteger)getStartPageLastVisitedGroupIndex {
  PrefService *prefService = [VivaldiStartPagePrefs localPrefService];
  return prefService->GetInteger(
            vivaldiprefs::kVivaldiStartPageLastVisitedGroup);
}

+ (BOOL)showStartPageCustomizeButton {
  PrefService *prefService = [VivaldiStartPagePrefs prefService];
  return prefService->GetBoolean(
      vivaldiprefs::kVivaldiStartPageShowCustomizeButton);
}

+ (NSString*)getWallpaperName {
  PrefService *prefService = [VivaldiStartPagePrefs prefService];
  NSString *name = base::SysUTF8ToNSString(
      prefService->GetString(vivaldiprefs::kVivaldiStartupWallpaper));
  return name;
}

+ (UIImage *)getPortraitWallpaper {
  PrefService *prefService = [VivaldiStartPagePrefs prefService];
  NSString *base64String =
  base::SysUTF8ToNSString(
    prefService->GetString(vivaldiprefs::kVivaldiStartpagePortraitImage)
  );
  return [self getImageFromBase64String:base64String];
}

+ (UIImage *)getLandscapeWallpaper {
  PrefService *prefService = [VivaldiStartPagePrefs prefService];
  NSString *base64String =
  base::SysUTF8ToNSString(
    prefService->GetString(vivaldiprefs::kVivaldiStartpageLandscapeImage)
  );
  return [self getImageFromBase64String:base64String];
}

#pragma mark - SETTERS

+ (void)setSDSortingMode:(const SpeedDialSortingMode)mode {
  PrefService *prefService = [VivaldiStartPagePrefs prefService];
  prefService->SetInteger(vivaldiprefs::kVivaldiSpeedDialSortingMode, mode);
}

+ (void)setSDSortingOrder:(const SpeedDialSortingOrder)order {
  PrefService *prefService = [VivaldiStartPagePrefs prefService];
  prefService->SetInteger(vivaldiprefs::kVivaldiSpeedDialSortingOrder, order);
}

+ (void)setStartPageLayoutStyle:(const VivaldiStartPageLayoutStyle)style {
  PrefService *prefService = [VivaldiStartPagePrefs prefService];
  prefService->SetInteger(vivaldiprefs::kVivaldiStartPageLayoutStyle, style);
}

+ (void)setStartPageSpeedDialMaximumColumns:
    (VivaldiStartPageLayoutColumn)columns {
  PrefService *prefService = [VivaldiStartPagePrefs prefService];
  prefService->SetInteger(vivaldiprefs::kVivaldiStartPageSDMaximumColumns,
                          columns);
}

+ (void)setShowFrequentlyVisitedPages:(BOOL)show {
  PrefService *prefService = [VivaldiStartPagePrefs prefService];
  prefService->SetBoolean(
      vivaldiprefs::kVivaldiStartPageShowFrequentlyVisited, show);
}

+ (void)setShowSpeedDials:(BOOL)show {
  PrefService *prefService = [VivaldiStartPagePrefs prefService];
  prefService->SetBoolean(
      vivaldiprefs::kVivaldiStartPageShowSpeedDials, show);
}

+ (void)setReopenStartPageWithItem:(const VivaldiStartPageStartItemType)item {
  PrefService *prefService = [VivaldiStartPagePrefs localPrefService];
  prefService->SetInteger(vivaldiprefs::kVivaldiStartPageOpenWithItem,
                          item);
}

+ (void)setStartPageLastVisitedGroupIndex:(const NSInteger)index {
  PrefService *prefService = [VivaldiStartPagePrefs localPrefService];
  prefService->SetInteger(vivaldiprefs::kVivaldiStartPageLastVisitedGroup,
                          index);
}

+ (void)setShowStartPageCustomizeButton:(BOOL)show {
  PrefService *prefService = [VivaldiStartPagePrefs prefService];
  prefService->SetBoolean(
      vivaldiprefs::kVivaldiStartPageShowCustomizeButton, show);
}

+ (void)setWallpaperName:(NSString*)name {
  PrefService *prefService = [VivaldiStartPagePrefs prefService];
  prefService->SetString(
      vivaldiprefs::kVivaldiStartupWallpaper, base::SysNSStringToUTF8(name));
}

+ (void)setPortraitWallpaper:(UIImage *)image {
  [self storeImageAsBase64String:image forPreferenceKey:
    vivaldiprefs::kVivaldiStartpagePortraitImage];
}

+ (void)setLandscapeWallpaper:(UIImage *)image {
  [self storeImageAsBase64String:image forPreferenceKey:
    vivaldiprefs::kVivaldiStartpageLandscapeImage];
}

#pragma mark - PRIVATE

// Return unlimited column by default which means fill the available screen
+ (VivaldiStartPageLayoutColumn)defaultColumns {
  return VivaldiStartPageLayoutColumnUnlimited;
}

+ (void)storeImageAsBase64String:(UIImage *)image
                forPreferenceKey:(const std::string &)preferenceKey {
  // Convert UIImage to NSData
  NSData *imageData = UIImagePNGRepresentation(image);
  // Encode NSData to Base64 string
  NSString *base64String = [imageData base64EncodedStringWithOptions:0];
  // Store the Base64 string
  PrefService *prefService = [VivaldiStartPagePrefs prefService];
  prefService->SetString(preferenceKey, base::SysNSStringToUTF8(base64String));
}

+ (UIImage *)getImageFromBase64String:(NSString *)base64String {
  // Decode Base64 string to NSData
  NSData *imageData =
    [[NSData alloc] initWithBase64EncodedString:base64String options:0];
  // Convert NSData to UIImage
  UIImage *image = [UIImage imageWithData:imageData];
  return image;
}

@end
