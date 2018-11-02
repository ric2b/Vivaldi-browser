// Copyright (c) 2016 Vivaldi Technologies. All rights reserved.

#include "prefs/native_settings_observer.h"

#include <string>

#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"

namespace vivaldi {

NativeSettingsObserver::NativeSettingsObserver(Profile* profile)
    : profile_(profile) {
  DCHECK(profile_);
}

void NativeSettingsObserver::SetPref(const char* name, const int value) {
  profile_->GetPrefs()->SetInteger(name, value);
}
void NativeSettingsObserver::SetPref(const char* name,
                                     const std::string& value) {
  profile_->GetPrefs()->SetString(name, value);
}
void NativeSettingsObserver::SetPref(const char* name, const bool value) {
  profile_->GetPrefs()->SetBoolean(name, value);
}

}  // namespace vivaldi
