// Copyright (c) 2017 Vivaldi Technologies

#ifndef PREFS_NATIVE_SETTINGS_OBSERVER_WIN_H_
#define PREFS_NATIVE_SETTINGS_OBSERVER_WIN_H_

#include "prefs/native_settings_observer.h"

class Profile;

namespace vivaldi {

class NativeSettingsObserverWin : public NativeSettingsObserver {
 public:
  explicit NativeSettingsObserverWin(Profile* profile);
  ~NativeSettingsObserverWin() override = default;
};

}  // namespace vivaldi

#endif  // PREFS_NATIVE_SETTINGS_OBSERVER_WIN_H_
