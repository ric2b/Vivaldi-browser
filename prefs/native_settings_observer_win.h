// Copyright (c) 2017 Vivaldi Technologies

#ifndef PREFS_NATIVE_SETTINGS_OBSERVER_WIN_H_
#define PREFS_NATIVE_SETTINGS_OBSERVER_WIN_H_

#include "base/win/registry.h"
#include "prefs/native_settings_observer.h"

class Profile;

namespace vivaldi {

class NativeSettingsObserverWin : public NativeSettingsObserver {
 public:
  explicit NativeSettingsObserverWin(Profile* profile);
  ~NativeSettingsObserverWin() override;

  void OnThemeColorUpdated();

 private:
 std::unique_ptr<base::win::RegKey> theme_key_;
};

}  // namespace vivaldi

#endif  // PREFS_NATIVE_SETTINGS_OBSERVER_WIN_H_
