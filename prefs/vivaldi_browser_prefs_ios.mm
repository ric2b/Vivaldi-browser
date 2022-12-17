// Copyright (c) 2022 Vivaldi Technologies

#include "prefs/vivaldi_browser_prefs.h"

#include "base/values.h"

namespace vivaldi {

void RegisterOldPlatformPrefs(user_prefs::PrefRegistrySyncable* registry) {
}

void MigrateOldPlatformPrefs(PrefService* prefs) {
}

base::Value GetPlatformComputedDefault(const std::string& path) {
  return base::Value();
}

}  // namespace vivaldi
