// Copyright (c) 2016 Vivaldi Technologies. All Rights Reserverd.

#include "prefs/native_settings_observer_mac.h"

#import <Cocoa/Cocoa.h>
#import <CoreFoundation/CoreFoundation.h>

#include "base/mac/sdk_forward_declarations.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/cocoa/last_active_browser_cocoa.h"
#include "components/prefs/pref_service.h"
#include "vivaldi/prefs/vivaldi_gen_prefs.h"
#include "vivaldi/prefs/vivaldi_gen_pref_enums.h"

namespace vivaldi {

// static
NativeSettingsObserver* NativeSettingsObserver::Create(Profile* profile) {
  return new NativeSettingsObserverMac(profile);
}

void AquaColorChanged(CFNotificationCenterRef center,
                      void* observer,
                      CFStringRef name,
                      const void* object,
                      CFDictionaryRef userInfo) {
  NSUserDefaults* userDefaults = [NSUserDefaults standardUserDefaults];

  NSNumber* appleAquaColorVariant =
      [userDefaults objectForKey:@"AppleAquaColorVariant"];

  reinterpret_cast<NativeSettingsObserver*>(observer)->SetPref(
      vivaldiprefs::kSystemMacAquaColorVariant,
      [appleAquaColorVariant intValue]);
}

void NoRedisplayChanged(CFNotificationCenterRef center,
                        void* observer,
                        CFStringRef name,
                        const void* object,
                        CFDictionaryRef userInfo) {
  NSUserDefaults* userDefaults = [NSUserDefaults standardUserDefaults];

  NSString* appleActionOnDoubleClick =
      [userDefaults stringForKey:@"AppleActionOnDoubleClick"];
  std::string val = "";
  if (appleActionOnDoubleClick) {
    val = [appleActionOnDoubleClick UTF8String];
  }

  reinterpret_cast<NativeSettingsObserver*>(observer)
      ->SetPref(vivaldiprefs::kSystemMacActionOnDoubleClick, val);
}

void SwipeDirectionChanged(CFNotificationCenterRef center,
                           void* observer,
                           CFStringRef name,
                           const void* object,
                           CFDictionaryRef userInfo) {
  NSUserDefaults* userDefaults = [NSUserDefaults standardUserDefaults];

  bool appleSwipeScrollDirection =
      [[userDefaults objectForKey:@"com.apple.swipescrolldirection"] boolValue];
  reinterpret_cast<NativeSettingsObserver*>(observer)
      ->SetPref(vivaldiprefs::kSystemMacSwipeScrollDirection,
                appleSwipeScrollDirection);
}

void SystemDarkModeChanged(CFNotificationCenterRef center,
                           void* observer,
                           CFStringRef name,
                           const void* object,
                           CFDictionaryRef userInfo) {
  vivaldiprefs::SystemDesktopThemeColorValues theme_color =
      vivaldiprefs::SystemDesktopThemeColorValues::LIGHT;
  NSString *osxMode =
    [[NSUserDefaults standardUserDefaults] stringForKey:@"AppleInterfaceStyle"];
  if (osxMode && [osxMode isEqual:@"Dark"]) {
    theme_color = vivaldiprefs::SystemDesktopThemeColorValues::DARK;
  }
  reinterpret_cast<NativeSettingsObserver*>(observer)->SetPref(
      vivaldiprefs::kSystemDesktopThemeColor, static_cast<int>(theme_color));
}

NativeSettingsObserverMac::NativeSettingsObserverMac(Profile* profile)
    : NativeSettingsObserver(profile) {

  // Initialize, in case the values are changed while Vivaldi is not running.
  AquaColorChanged(CFNotificationCenterGetDistributedCenter(), this,
    CFSTR("AppleAquaColorVariantChanged"), nullptr, nullptr);

  SwipeDirectionChanged(CFNotificationCenterGetDistributedCenter(), this,
    CFSTR("SwipeScrollDirectionDidChangeNotification"), nullptr, nullptr);

  NoRedisplayChanged(CFNotificationCenterGetDistributedCenter(), this,
    CFSTR("AppleNoRedisplayAppearancePreferenceChanged"), nullptr, nullptr);

  SystemDarkModeChanged(CFNotificationCenterGetDistributedCenter(), this,
    CFSTR("AppleInterfaceThemeChangedNotification"), nullptr, nullptr);

  // NOTE(tomas@vivaldi.com): fix for VB-39486
  CFNotificationCenterRemoveEveryObserver(
      CFNotificationCenterGetDistributedCenter(), this);


  CFNotificationCenterAddObserver(
      CFNotificationCenterGetDistributedCenter(), this, AquaColorChanged,
      CFSTR("AppleAquaColorVariantChanged"), NULL,
      CFNotificationSuspensionBehaviorDeliverImmediately);
  CFNotificationCenterAddObserver(
      CFNotificationCenterGetDistributedCenter(), this,
      SwipeDirectionChanged, CFSTR("SwipeScrollDirectionDidChangeNotification"),
      NULL, CFNotificationSuspensionBehaviorDeliverImmediately);
  CFNotificationCenterAddObserver(
      CFNotificationCenterGetDistributedCenter(), this, NoRedisplayChanged,
      CFSTR("AppleNoRedisplayAppearancePreferenceChanged"), NULL,
      CFNotificationSuspensionBehaviorDeliverImmediately);
  CFNotificationCenterAddObserver(
      CFNotificationCenterGetDistributedCenter(), this, SystemDarkModeChanged,
      CFSTR("AppleInterfaceThemeChangedNotification"), NULL,
      CFNotificationSuspensionBehaviorDeliverImmediately);
}

NativeSettingsObserverMac::~NativeSettingsObserverMac() {
  CFNotificationCenterRemoveEveryObserver(
      CFNotificationCenterGetDistributedCenter(), this);
}

}  // namespace vivaldi
