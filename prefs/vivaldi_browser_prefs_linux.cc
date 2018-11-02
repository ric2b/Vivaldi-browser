// Copyright 2015 Vivaldi Technologies

#include "prefs/vivaldi_browser_prefs.h"

#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "prefs/vivaldi_pref_names.h"
#include "vivaldi/prefs/vivaldi_gen_prefs.h"

namespace vivaldi {

void RegisterOldPlatformPrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      vivaldiprefs::kOldHideMouseCursorInFullscreen, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}

void MigrateOldPlatformPrefs(PrefService* prefs) {
  {
    const base::Value* old_hide_mouse_cursor_in_full_screen_pref =
        prefs->GetUserPrefValue(vivaldiprefs::kOldHideMouseCursorInFullscreen);
    if (old_hide_mouse_cursor_in_full_screen_pref)
      prefs->Set(vivaldiprefs::kWebpagesFullScreenHideMouse,
                 *old_hide_mouse_cursor_in_full_screen_pref);
    prefs->ClearPref(vivaldiprefs::kOldHideMouseCursorInFullscreen);
  }
}

std::unique_ptr<base::Value> GetPlatformComputedDefault(
    const std::string& path) {
  return std::make_unique<base::Value>();
}

std::string GetPlatformDefaultKey() {
  return "default_linux";
}

}  // namespace vivaldi
