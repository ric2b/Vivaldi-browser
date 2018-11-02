// Copyright (c) 2016 Vivaldi Technologies. All Rights Reserved.

#include "prefs/native_settings_observer_linux.h"

#include "base/logging.h"

namespace vivaldi {

// static
NativeSettingsObserver* NativeSettingsObserver::Create(Profile* profile) {
  return new NativeSettingsObserverLinux(profile);
}

NativeSettingsObserverLinux::NativeSettingsObserverLinux(Profile* profile)
    : NativeSettingsObserver(profile) {}

}  // namespace vivaldi
