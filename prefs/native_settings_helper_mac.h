// Copyright (c) 2020 Vivaldi Technologies. All Rights Reserved.

#ifndef PREFS_NATIVE_SETTINGS_HELPER_MAC_H_
#define PREFS_NATIVE_SETTINGS_HELPER_MAC_H_

#include <string>

namespace vivaldi {

std::string getActionOnDoubleClick();
int getKeyboardUIMode();
bool getSwipeDirection();
int getSystemDarkMode();
std::string getSystemAccentColor();
std::string getSystemHighlightColor();

}  // namespace vivaldi

#endif  // PREFS_VIVALDI_BROWSERPREFS_UTIL_H_
