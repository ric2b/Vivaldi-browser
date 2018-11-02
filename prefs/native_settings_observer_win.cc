// Copyright (c) 2016 Vivaldi Technologies. All Rights Reserved.

#include "base/logging.h"
#include "prefs/native_settings_observer_win.h"

namespace vivaldi {

// static
NativeSettingsObserver* NativeSettingsObserver::Create(Profile* profile) {
  return new NativeSettingsObserverWin(profile);
}

NativeSettingsObserverWin::NativeSettingsObserverWin(Profile* profile)
    : NativeSettingsObserver(profile) {}

}  // namespace vivaldi
