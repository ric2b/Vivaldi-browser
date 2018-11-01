// Copyright (c) 2016 Vivaldi Technologies. All Rights Reserved.

#include "prefs/native_settings_observer_linux.h"

namespace vivaldi {

// static
NativeSettingsObserver* NativeSettingsObserver::Create(
    NativeSettingsObserverDelegate* delegate) {
  return new NativeSettingsObserverLinux(delegate);
}

NativeSettingsObserverLinux::NativeSettingsObserverLinux(
    NativeSettingsObserverDelegate* delegate)
    : delegate_(delegate) {
  DCHECK(delegate_);
}

NativeSettingsObserverLinux::~NativeSettingsObserverLinux() {}

}  // namespace vivaldi
