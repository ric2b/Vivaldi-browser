// Copyright (c) 2016 Vivaldi Technologies. All Rights Reserved.

#include "prefs/native_settings_observer_win.h"

namespace vivaldi {

// static
NativeSettingsObserver* NativeSettingsObserver::Create(
    NativeSettingsObserverDelegate* delegate) {
  return new NativeSettingsObserverWin(delegate);
}

NativeSettingsObserverWin::NativeSettingsObserverWin(
    NativeSettingsObserverDelegate* delegate)
    : delegate_(delegate) {
  DCHECK(delegate_);
}

NativeSettingsObserverWin::~NativeSettingsObserverWin() {}

}  // namespace vivaldi
