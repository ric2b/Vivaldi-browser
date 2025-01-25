// Copyright (c) 2020 Vivaldi Technologies. All Rights Reserverd.

#include "prefs/native_settings_helper_mac.h"

#import <Cocoa/Cocoa.h>

#include "base/strings/stringprintf.h"
#include "base/strings/string_split.h"
#include "base/strings/string_number_conversions.h"
#include "vivaldi/prefs/vivaldi_gen_pref_enums.h"

namespace vivaldi {

namespace {

  std::string RgbToHexString(double red, double green, double blue) {
  // red, green and blue have values between 0.0 and 1.0
  return base::StringPrintf("%02X%02X%02X",
      static_cast<int>(255.999f*red),
      static_cast<int>(255.999f*green),
      static_cast<int>(255.999f*blue));
  }

  bool IsDarkMode() {
    NSAppearanceName appearance =
        [NSApp.effectiveAppearance bestMatchFromAppearancesWithNames:@[
          NSAppearanceNameAqua, NSAppearanceNameDarkAqua
        ]];
    return [appearance isEqual:NSAppearanceNameDarkAqua];
  }

} // namespace

std::string getActionOnDoubleClick() {
  NSString* appleActionOnDoubleClick =
    [[NSUserDefaults standardUserDefaults]
      stringForKey:@"AppleActionOnDoubleClick"];
  std::string val = "";
  if (appleActionOnDoubleClick) {
    val = [appleActionOnDoubleClick UTF8String];
  }
  return val;
}

int getKeyboardUIMode() {
  NSNumber* appleKeyboardUIMode =
    [[NSUserDefaults standardUserDefaults] objectForKey:@"AppleKeyboardUIMode"];
  return [appleKeyboardUIMode intValue];
}

bool getSwipeDirection() {
  bool appleSwipeScrollDirection =
    [[[NSUserDefaults standardUserDefaults]
      objectForKey:@"com.apple.swipescrolldirection"] boolValue];
  return appleSwipeScrollDirection;
}

int getSystemDarkMode() {
  vivaldiprefs::SystemDesktopThemeColorValues theme_color =
      IsDarkMode() ? vivaldiprefs::SystemDesktopThemeColorValues::kDark
                   : vivaldiprefs::SystemDesktopThemeColorValues::kLight;

  return static_cast<int>(theme_color);
}

std::string getSystemAccentColor() {
  std::string accentColorString = "";
  if (@available(macOS 10.14, *)) {
    NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
    if ([defaults stringForKey:@"AppleAccentColor"]) {
      NSColor* c = [[NSColor controlAccentColor]
          colorUsingColorSpace:[NSColorSpace deviceRGBColorSpace]];
      accentColorString =
          RgbToHexString(c.redComponent,c.greenComponent,c.blueComponent);
    }
  }
  return accentColorString;
}

std::string getSystemHighlightColor() {
  NSString* hightlightColorPref =
    [[NSUserDefaults standardUserDefaults] stringForKey:@"AppleHighlightColor"];
  std::string hightlightColorString = "";
  if (hightlightColorPref != nil) {
    std::vector<std::string> colors = base::SplitString(
      [hightlightColorPref UTF8String], " ",
      base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

    double red, green, blue;
    if (colors.size() >= 3 &&
        base::StringToDouble(colors[0], &red) &&
        base::StringToDouble(colors[1], &green) &&
        base::StringToDouble(colors[2], &blue)) {
      hightlightColorString = RgbToHexString(red, green, blue);
    }
  }
  return hightlightColorString;
}

}  // namespace vivaldi
