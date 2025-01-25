// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_SYNC_SESSIONS_VIV_SPECIFIC_OBSERVER_H_
#define COMPONENTS_SYNC_SESSIONS_VIV_SPECIFIC_OBSERVER_H_

#include "chromium/components/prefs/pref_change_registrar.h"

class Profile;

namespace vivaldi {

class VivSpecificObserver {
 public:
  explicit VivSpecificObserver(Profile* profile);
  virtual ~VivSpecificObserver();

  void TriggerSync();

 private:
  void OnPrefsChanged(const std::string& path);

  PrefChangeRegistrar prefs_registrar_;
  const raw_ptr<Profile> profile_;
};

} // namespace vivaldi

#endif // COMPONENTS_SYNC_SESSIONS_VIV_SPECIFIC_OBSERVER_H_
