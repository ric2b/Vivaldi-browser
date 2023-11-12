// Copyright (c) 2016-2021 Vivaldi Technologies AS. All Rights Reserved.

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "prefs/vivaldi_pref_names.h"

#include "prefs/native_settings_helper_win.h"
#include "prefs/native_settings_observer_win.h"
#include "vivaldi/prefs/vivaldi_gen_pref_enums.h"
#include "vivaldi/prefs/vivaldi_gen_prefs.h"

namespace vivaldi {

// static
NativeSettingsObserver* NativeSettingsObserver::Create(Profile* profile) {
  return new NativeSettingsObserverWin(profile);
}

NativeSettingsObserverWin::~NativeSettingsObserverWin() {
}

NativeSettingsObserverWin::NativeSettingsObserverWin(Profile* profile)
    : NativeSettingsObserver(profile) {
  theme_key_.reset(new base::win::RegKey(
      HKEY_CURRENT_USER,
      L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
      KEY_READ));
  if (theme_key_->Valid()) {
    OnThemeColorUpdated();
  }
}

void NativeSettingsObserverWin::OnThemeColorUpdated() {
  DWORD ubr = 0;
  vivaldiprefs::SystemDesktopThemeColorValues theme_color =
      vivaldiprefs::SystemDesktopThemeColorValues::kLight;
  if (theme_key_->ReadValueDW(L"AppsUseLightTheme", &ubr) == ERROR_SUCCESS) {
    if (ubr == 0) {
      // 0 is dark.
      theme_color = vivaldiprefs::SystemDesktopThemeColorValues::kDark;
    }
  }
  SetPref(vivaldiprefs::kSystemDesktopThemeColor,
          static_cast<int>(theme_color));

  // Watch for future changes. base::Unretained(this) because theme_key_
  // is valid as long as |this| is.
  if (!theme_key_->StartWatching(
          base::BindOnce(&NativeSettingsObserverWin::OnThemeColorUpdated,
                         base::Unretained(this)))) {
    theme_key_.reset();
  }
}

void NativeSettingsObserverWin::OnSysColorChange() {
  SetPref(vivaldiprefs::kSystemAccentColor, GetSystemAccentColor());
  SetPref(vivaldiprefs::kSystemHighlightColor, GetSystemHighlightColor());
}

}  // namespace vivaldi
