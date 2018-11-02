// Copyright (c) 2015 Vivaldi Technologies

#ifndef PREFS_NATIVE_SETTINGS_OBSERVER_LINUX_H_
#define PREFS_NATIVE_SETTINGS_OBSERVER_LINUX_H_

#include "prefs/native_settings_observer.h"

class Profile;

namespace vivaldi {

class NativeSettingsObserverLinux : public NativeSettingsObserver {
 public:
  explicit NativeSettingsObserverLinux(Profile* profile);
  ~NativeSettingsObserverLinux() override = default;
};

}  // namespace vivaldi

#endif  // PREFS_NATIVE_SETTINGS_OBSERVER_LINUX_H_
